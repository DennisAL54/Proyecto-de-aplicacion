#include "motion.h"
#include <string.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
static void *reservar(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
#else
static void *reservar(size_t n) { return malloc(n); }
#endif

// ----------------------------- parámetros -----------------------------
#define MD_RGB565_INVERTIDO 0          // use el MISMO valor que RGB565_INVERTIDO en captura.cpp
static const int UMBRAL = 50;          // diferencia mínima de gris para contar como "cambio"
static const int AREA_MIN = 14;        // píxeles mínimos de una mancha (a 160x120)
static const int AREA_MAX = (MD_W * MD_H) / 4;   // una mancha mayor es un cambio de luz, no un vehículo
static const int FRAC_GLOBAL_PCT = 40; // si cambia más de este % del cuadro: cambio global de luz
static const int CUADROS_INICIO = 10;  // cuadros de aprendizaje rápido del fondo
static const int SHIFT_INICIO = 2;     // aprendizaje rápido: 1/4 por cuadro
static const int SHIFT_NORMAL = 5;     // aprendizaje lento: 1/32 por cuadro
static const int FUSION_PX = 3;        // une fragmentos de un mismo vehículo separados menos de esto
static const int PILA_MAX = 4096;
// -----------------------------------------------------------------------

#define NPX (MD_W * MD_H)

static uint8_t *gris, *mascara, *tmp, *trabajo;
static uint16_t *fondo16;              // fondo con 8 bits fraccionarios
static uint16_t *pila;
static int n_cuadro;

bool md_init()
{
    gris = (uint8_t *)reservar(NPX);
    mascara = (uint8_t *)reservar(NPX);
    tmp = (uint8_t *)reservar(NPX);
    trabajo = (uint8_t *)reservar(NPX);
    fondo16 = (uint16_t *)reservar(NPX * sizeof(uint16_t));
    pila = (uint16_t *)reservar(PILA_MAX * sizeof(uint16_t));
    if (!gris || !mascara || !tmp || !trabajo || !fondo16 || !pila) return false;
    memset(mascara, 0, NPX);
    n_cuadro = 0;
    return true;
}

const uint8_t *md_gris() { return gris; }
const uint8_t *md_mascara() { return mascara; }

// ---- 1) imagen reducida en escala de grises ----
static inline int lum565(const uint8_t *p)
{
#if MD_RGB565_INVERTIDO
    uint16_t v = (uint16_t)((p[1] << 8) | p[0]);
#else
    uint16_t v = (uint16_t)((p[0] << 8) | p[1]);
#endif
    int r = ((v >> 11) & 0x1F) << 3;
    int g = ((v >> 5) & 0x3F) << 2;
    int b = (v & 0x1F) << 3;
    return (77 * r + 150 * g + 29 * b) >> 8;
}

static void reducir(const uint8_t *src, int ancho)
{
    for (int y = 0; y < MD_H; y++) {
        const uint8_t *f0 = src + (2 * y) * ancho * 2;      // fila 2y
        const uint8_t *f1 = f0 + ancho * 2;                 // fila 2y+1
        for (int x = 0; x < MD_W; x++) {
            int s = lum565(f0 + 4 * x) + lum565(f0 + 4 * x + 2) + lum565(f1 + 4 * x) + lum565(f1 + 4 * x + 2);
            gris[y * MD_W + x] = (uint8_t)(s >> 2);
        }
    }
}

// ---- 2) cambio global de brillo: mediana de (imagen - fondo) ----
static int desfase_global()
{
    static int hist[511];
    memset(hist, 0, sizeof(hist));
    int cuenta = 0;
    for (int i = 0; i < NPX; i += 4) {
        int d = (int)gris[i] - (int)(fondo16[i] >> 8);
        hist[d + 255]++;
        cuenta++;
    }
    int acum = 0, mitad = cuenta / 2;
    for (int k = 0; k < 511; k++) {
        acum += hist[k];
        if (acum > mitad) return k - 255;
    }
    return 0;
}

// ---- 3) morfología separable (erosión o dilatación con ventana de radio r) ----
static void morfo(const uint8_t *src, uint8_t *dst, int r, bool dilatar)
{
    for (int y = 0; y < MD_H; y++) {                         // pasada horizontal: src -> trabajo
        const uint8_t *fila = src + y * MD_W;
        uint8_t *o = trabajo + y * MD_W;
        for (int x = 0; x < MD_W; x++) {
            int x0 = x - r < 0 ? 0 : x - r, x1 = x + r >= MD_W ? MD_W - 1 : x + r;
            uint8_t v = dilatar ? 0 : 1;
            for (int k = x0; k <= x1; k++) {
                if (dilatar) { if (fila[k]) { v = 1; break; } }
                else         { if (!fila[k]) { v = 0; break; } }
            }
            o[x] = v;
        }
    }
    for (int x = 0; x < MD_W; x++) {                         // pasada vertical: trabajo -> dst
        for (int y = 0; y < MD_H; y++) {
            int y0 = y - r < 0 ? 0 : y - r, y1 = y + r >= MD_H ? MD_H - 1 : y + r;
            uint8_t v = dilatar ? 0 : 1;
            for (int k = y0; k <= y1; k++) {
                uint8_t m = trabajo[k * MD_W + x];
                if (dilatar) { if (m) { v = 1; break; } }
                else         { if (!m) { v = 0; break; } }
            }
            dst[y * MD_W + x] = v;
        }
    }
}

// ---- 4) manchas por relleno (flood fill de 8 vecinos) ----
struct Caja { int x0, y0, x1, y1, area; };

static int buscar_cajas(Caja *cajas, int max)
{
    memcpy(trabajo, mascara, NPX);
    int n = 0;
    for (int i = 0; i < NPX; i++) {
        if (!trabajo[i]) continue;
        int sp = 0, area = 0;
        int minx = MD_W, miny = MD_H, maxx = -1, maxy = -1;
        pila[sp++] = (uint16_t)i;
        trabajo[i] = 0;
        while (sp) {
            int p = pila[--sp];
            int px = p % MD_W, py = p / MD_W;
            area++;
            if (px < minx) minx = px;
            if (px > maxx) maxx = px;
            if (py < miny) miny = py;
            if (py > maxy) maxy = py;
            for (int dy = -1; dy <= 1; dy++) {
                int ny = py + dy;
                if (ny < 0 || ny >= MD_H) continue;
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = px + dx;
                    if (nx < 0 || nx >= MD_W) continue;
                    int q = ny * MD_W + nx;
                    if (trabajo[q] && sp < PILA_MAX) { trabajo[q] = 0; pila[sp++] = (uint16_t)q; }
                }
            }
        }
        if (area >= AREA_MIN && n < max) cajas[n++] = Caja{minx, miny, maxx, maxy, area};
    }
    return n;
}

// Une fragmentos de un mismo vehículo: cajas apiladas verticalmente (hueco <= FUSION_PX) que comparten
// al menos la mitad del ancho de la menor. No une vehículos de carriles distintos.
static int fusionar(Caja *c, int n)
{
    bool cambio = true;
    while (cambio) {
        cambio = false;
        for (int i = 0; i < n && !cambio; i++) {
            for (int j = i + 1; j < n; j++) {
                int wa = c[i].x1 - c[i].x0 + 1, wb = c[j].x1 - c[j].x0 + 1;
                int solape = (c[i].x1 < c[j].x1 ? c[i].x1 : c[j].x1) - (c[i].x0 > c[j].x0 ? c[i].x0 : c[j].x0) + 1;
                int hueco = (c[i].y0 > c[j].y0 ? c[i].y0 : c[j].y0) - (c[i].y1 < c[j].y1 ? c[i].y1 : c[j].y1) - 1;
                int menor = wa < wb ? wa : wb;
                if (2 * solape >= menor && hueco <= FUSION_PX) {
                    c[i].x0 = c[i].x0 < c[j].x0 ? c[i].x0 : c[j].x0;
                    c[i].y0 = c[i].y0 < c[j].y0 ? c[i].y0 : c[j].y0;
                    c[i].x1 = c[i].x1 > c[j].x1 ? c[i].x1 : c[j].x1;
                    c[i].y1 = c[i].y1 > c[j].y1 ? c[i].y1 : c[j].y1;
                    c[i].area += c[j].area;
                    c[j] = c[n - 1];
                    n--;
                    cambio = true;
                    break;
                }
            }
        }
    }
    return n;
}

int md_process(const uint8_t *rgb565, int ancho, int alto, Blob *out, int max_out, MotionStats *st)
{
    if (ancho != 2 * MD_W || alto != 2 * MD_H) return -1;
    reducir(rgb565, ancho);

    if (n_cuadro == 0) {                                      // el primer cuadro es el fondo inicial
        for (int i = 0; i < NPX; i++) fondo16[i] = (uint16_t)(gris[i] << 8);
        memset(mascara, 0, NPX);
        if (st) { st->frame = 0; st->desfase = 0; st->px_mov = 0; st->cambio_global = false; }
        n_cuadro = 1;
        return 0;
    }

    int off = desfase_global();
    for (int i = 0; i < NPX; i++) {
        int d = (int)gris[i] - (int)(fondo16[i] >> 8) - off;
        if (d < 0) d = -d;
        mascara[i] = d > UMBRAL;
    }
    morfo(mascara, tmp, 1, false);  morfo(tmp, mascara, 1, true);    // apertura 3x3: quita puntos aislados
    morfo(mascara, tmp, 2, true);   morfo(tmp, mascara, 2, false);   // cierre 5x5: une partes del vehículo

    int px = 0;
    for (int i = 0; i < NPX; i++) px += mascara[i];
    bool global = px * 100 > FRAC_GLOBAL_PCT * NPX;
    if (st) { st->frame = n_cuadro; st->desfase = off; st->px_mov = px; st->cambio_global = global; }

    // ---- actualización del fondo (solo donde no hay movimiento) ----
    if (global) {                                              // cambio brusco de luz: reaprender rápido
        for (int i = 0; i < NPX; i++) {
            int32_t d = ((int32_t)gris[i] << 8) - (int32_t)fondo16[i];
            fondo16[i] = (uint16_t)((int32_t)fondo16[i] + (d >> 1));
        }
        n_cuadro++;
        return 0;
    }
    morfo(mascara, tmp, 2, true);                              // zona de movimiento ampliada
    int sh = n_cuadro < CUADROS_INICIO ? SHIFT_INICIO : SHIFT_NORMAL;
    for (int i = 0; i < NPX; i++) {
        if (tmp[i]) continue;
        int32_t d = ((int32_t)gris[i] << 8) - (int32_t)fondo16[i];
        fondo16[i] = (uint16_t)((int32_t)fondo16[i] + (d >> sh));
    }
    n_cuadro++;

    // ---- manchas ----
    Caja cajas[MD_MAX_BLOBS * 3];
    int n = buscar_cajas(cajas, MD_MAX_BLOBS * 3);
    n = fusionar(cajas, n);
    int nb = 0;
    for (int i = 0; i < n && nb < max_out; i++) {
        int w = cajas[i].x1 - cajas[i].x0 + 1, h = cajas[i].y1 - cajas[i].y0 + 1;
        if (w * h > AREA_MAX) continue;
        // Punto de contacto con el suelo: centro de las últimas filas de la mancha. Si otro vehículo más
        // lejano se solapa por encima, la posición sigue siendo la del más cercano.
        int r0 = cajas[i].y1 - 2 < cajas[i].y0 ? cajas[i].y0 : cajas[i].y1 - 2;
        int c0 = MD_W, c1 = -1;
        for (int y = r0; y <= cajas[i].y1; y++)
            for (int x = cajas[i].x0; x <= cajas[i].x1; x++)
                if (mascara[y * MD_W + x]) { if (x < c0) c0 = x; if (x > c1) c1 = x; }
        float cx = c1 >= 0 ? (c0 + c1) / 2.0f : cajas[i].x0 + (w - 1) / 2.0f;
        Blob &b = out[nb++];
        b.x = cajas[i].x0; b.y = cajas[i].y0; b.w = w; b.h = h; b.area = cajas[i].area;
        b.gx = (cx + 0.5f) * 2.0f - 0.5f;                       // a coordenadas de la imagen original
        b.gy = (float)(cajas[i].y1 + 1) * 2.0f - 0.5f;
    }
    return nb;
}
