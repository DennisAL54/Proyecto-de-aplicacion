#include "yolo_det.h"
#include <math.h>
#include <string.h>

static const float ANCLAS[6][2] = {{12, 18}, {37, 49}, {52, 132}, {115, 73}, {119, 199}, {242, 238}};
static const int   MASCARAS[2][3] = {{3, 4, 5}, {0, 1, 2}};
#define MAX_CAND 96

static inline float sigmoide(float x) { return 1.0f / (1.0f + expf(-x)); }

static float iou(const YoloCaja &a, const YoloCaja &b)
{
    float ix = fminf(a.x1, b.x1) - fmaxf(a.x0, b.x0); if (ix < 0) ix = 0;
    float iy = fminf(a.y1, b.y1) - fmaxf(a.y0, b.y0); if (iy < 0) iy = 0;
    float i = ix * iy;
    return i / ((a.x1 - a.x0) * (a.y1 - a.y0) + (b.x1 - b.x0) * (b.y1 - b.y0) - i + 1e-9f);
}

static bool es_vehiculo(int c) { return c == YOLO_CAR || c == YOLO_MOTO || c == YOLO_BUS || c == YOLO_CAMION; }

static void cabeza(const YoloSalida &s, int g, const int *mascara, float umbral, bool solo_veh,
                   YoloCaja *cand, int *n)
{
    for (int y = 0; y < g; y++) {
        for (int x = 0; x < g; x++) {
            const int8_t *celda = s.datos + (y * g + x) * 255;
            for (int a = 0; a < 3; a++) {
                const int8_t *p = celda + a * 85;
                float obj = sigmoide(((int)p[4] - s.cero) * s.escala);
                if (obj < umbral) continue;                       // obj*clase <= obj: descarte rápido
                int mejor = -1; float mejor_p = 0;
                for (int k = 0; k < YOLO_CLASES; k++) {           // la clase es la de mayor probabilidad entre las 80
                    float c = sigmoide(((int)p[5 + k] - s.cero) * s.escala);
                    if (c > mejor_p) { mejor_p = c; mejor = k; }
                }
                float conf = obj * mejor_p;
                if (mejor < 0 || conf < umbral) continue;
                if (solo_veh && !es_vehiculo(mejor)) continue;     // si gana 'person', 'bicycle', etc., se descarta
                float tx = ((int)p[0] - s.cero) * s.escala, ty = ((int)p[1] - s.cero) * s.escala;
                float tw = ((int)p[2] - s.cero) * s.escala, th = ((int)p[3] - s.cero) * s.escala;
                float cx = (sigmoide(tx) + (float)x) / (float)g * YOLO_TAM;
                float cy = (sigmoide(ty) + (float)y) / (float)g * YOLO_TAM;
                float w = expf(tw) * ANCLAS[mascara[a]][0], h = expf(th) * ANCLAS[mascara[a]][1];
                YoloCaja c2 = {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, conf, mejor};
                if (*n < MAX_CAND) { cand[(*n)++] = c2; continue; }
                int peor = 0;                                    // lleno: reemplaza la de menor confianza si esta es mejor
                for (int i = 1; i < MAX_CAND; i++) if (cand[i].conf < cand[peor].conf) peor = i;
                if (conf > cand[peor].conf) cand[peor] = c2;
            }
        }
    }
}

int yolo_decodificar(const YoloSalida &cab10, const YoloSalida &cab20, float umbral, bool solo_vehiculos,
                     YoloCaja *out, int max_out)
{
    YoloCaja cand[MAX_CAND]; int n = 0;
    cabeza(cab10, 10, MASCARAS[0], umbral, solo_vehiculos, cand, &n);
    cabeza(cab20, 20, MASCARAS[1], umbral, solo_vehiculos, cand, &n);
    // NMS por clase (IoU 0.45), de mayor a menor confianza
    int usado[MAX_CAND] = {0}, m = 0;
    for (;;) {
        int mejor = -1;
        for (int i = 0; i < n; i++) if (!usado[i] && (mejor < 0 || cand[i].conf > cand[mejor].conf)) mejor = i;
        if (mejor < 0 || m >= max_out) break;
        usado[mejor] = 1; out[m++] = cand[mejor];
        for (int i = 0; i < n; i++)
            if (!usado[i] && cand[i].clase == cand[mejor].clase && iou(cand[i], cand[mejor]) > 0.45f) usado[i] = 1;
    }
    return m;
}

static void rellenar_gris(int8_t *entrada) { memset(entrada, 127 - 128, (size_t)YOLO_TAM * YOLO_TAM * 3); }  // gris 127 -> int8 -1

void yolo_entrada_rgb888(const uint8_t *rgb, int w, int h, int8_t *entrada, int *ox, int *oy)
{
    rellenar_gris(entrada);
    *ox = (YOLO_TAM - w) / 2; *oy = (YOLO_TAM - h) / 2;
    for (int y = 0; y < h; y++) {
        int8_t *d = entrada + ((y + *oy) * YOLO_TAM + *ox) * 3;
        const uint8_t *s = rgb + y * w * 3;
        for (int i = 0; i < w * 3; i++) d[i] = (int8_t)((int)s[i] - 128);
    }
}

void yolo_entrada_rgb565(const uint8_t *p, int w, int h, bool invertidos, int8_t *entrada, int *ox, int *oy)
{
    rellenar_gris(entrada);
    *ox = (YOLO_TAM - w) / 2; *oy = (YOLO_TAM - h) / 2;
    for (int y = 0; y < h; y++) {
        int8_t *d = entrada + ((y + *oy) * YOLO_TAM + *ox) * 3;
        for (int x = 0; x < w; x++) {
            const uint8_t *q = p + (y * w + x) * 2;
            uint16_t v = invertidos ? (uint16_t)((q[0] << 8) | q[1]) : (uint16_t)((q[1] << 8) | q[0]);
            int r = (v >> 11) & 0x1f, g = (v >> 5) & 0x3f, b = v & 0x1f;
            d[x * 3 + 0] = (int8_t)(((r << 3) | (r >> 2)) - 128);
            d[x * 3 + 1] = (int8_t)(((g << 2) | (g >> 4)) - 128);
            d[x * 3 + 2] = (int8_t)(((b << 3) | (b >> 2)) - 128);
        }
    }
}
