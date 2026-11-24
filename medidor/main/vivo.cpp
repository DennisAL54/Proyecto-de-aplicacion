#include "vivo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "camera_manager.h"
#include "motion.h"
#include "debug_bmp.h"
#include "homografia.h"
#include "cadena.h"

static const char *TAG = "vivo";
#define VIVO_MAX_IMG 15                   // imágenes de depuración que se guardan durante la medición
#define VIVO_SEP_IMG_S 4.0                // separación mínima entre imágenes (s)
#define VIVO_MAX_REG 8000                 // detecciones crudas que se guardan (unos 128 KB en PSRAM)

struct DetReg { float t, gx, gy; int cuadro; };

static Cadena C;                           // fuera de la pila


// Guarda el cuadro completo (320x240, color) como BMP, con una cruz en cada punto de calibración:
// 1 rojo, 2 verde, 3 azul, 4 amarillo. Sirve para revisar el encuadre y que los puntos caigan donde se pensó.
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

static bool guardar_encuadre(const char *ruta, const uint8_t *rgb565, int W, int H, const double (*pts)[2], int npts,
                             const int (*ign)[4], int nign)
{
    const uint32_t datos = (uint32_t)W * H * 3;          // 320*3 = 960 bytes por fila, múltiplo de 4
    uint8_t cab[54] = {};
    cab[0] = 'B'; cab[1] = 'M';
    put32(cab + 2, 54 + datos); put32(cab + 10, 54); put32(cab + 14, 40);
    put32(cab + 18, W); put32(cab + 22, H); put16(cab + 26, 1); put16(cab + 28, 24); put32(cab + 34, datos);
    FILE *f = fopen(ruta, "wb");
    if (!f) return false;
    fwrite(cab, 1, 54, f);
    uint8_t *fila = (uint8_t *)malloc(W * 3);
    if (!fila) { fclose(f); return false; }
    static const uint8_t col[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};   // R, G, B
    for (int y = H - 1; y >= 0; y--) {
        const uint8_t *p = rgb565 + (size_t)y * W * 2;
        for (int x = 0; x < W; x++) {
#if MD_RGB565_INVERTIDO
            uint16_t v = (uint16_t)((p[2 * x + 1] << 8) | p[2 * x]);
#else
            uint16_t v = (uint16_t)((p[2 * x] << 8) | p[2 * x + 1]);
#endif
            fila[3 * x + 2] = ((v >> 11) & 0x1F) << 3;     // R
            fila[3 * x + 1] = ((v >> 5) & 0x3F) << 2;      // G
            fila[3 * x]     = (v & 0x1F) << 3;             // B (el BMP guarda B, G, R)
        }
        for (int i = 0; i < npts && i < 4; i++) {
            int u = (int)(pts[i][0] + 0.5), w = (int)(pts[i][1] + 0.5);
            int dy = y - w;
            if (dy < -6 || dy > 6) continue;
            for (int x = u - 6; x <= u + 6; x++) {
                if (x < 0 || x >= W) continue;
                if (dy == 0 || x == u) { fila[3 * x] = col[i][2]; fila[3 * x + 1] = col[i][1]; fila[3 * x + 2] = col[i][0]; }
            }
        }
        for (int k = 0; k < nign; k++) {                  // zonas ignoradas: contorno en cian
            int x0 = ign[k][0], y0 = ign[k][1], x1 = ign[k][2], y1 = ign[k][3];
            if (y < y0 || y > y1) continue;
            for (int x = x0; x <= x1; x++) {
                if (x < 0 || x >= W) continue;
                if (y == y0 || y == y1 || x == x0 || x == x1) { fila[3 * x] = 255; fila[3 * x + 1] = 255; fila[3 * x + 2] = 0; }
            }
        }
        fwrite(fila, 1, W * 3, f);
    }
    free(fila);
    fflush(f);
    fclose(f);
    return true;
}

// Lee calibracion.txt: 4 líneas "u v X Y" y, opcional, "zona ymin ymax".
#define VIVO_MAX_IGN 4
static bool leer_calibracion(const char *ruta, Homografia *H, double *ymin, double *ymax, bool *hay_zona, double (*img)[2],
                             int (*ign)[4], int *nign)
{
    FILE *f = fopen(ruta, "r");
    if (!f) { ESP_LOGE(TAG, "falta %s", ruta); return false; }
    double mun[4][2];
    int n = 0; *hay_zona = false; *nign = 0;
    char linea[96];
    while (fgets(linea, sizeof linea, f)) {
        double a, b, c, d;
        if (n < 4 && sscanf(linea, "%lf %lf %lf %lf", &a, &b, &c, &d) == 4) {
            img[n][0] = a; img[n][1] = b; mun[n][0] = c; mun[n][1] = d; n++;
        } else if (sscanf(linea, "zona %lf %lf", &a, &b) == 2) {
            *ymin = a; *ymax = b; *hay_zona = true;
        } else {
            int x0, y0, x1, y1;
            if (*nign < VIVO_MAX_IGN && sscanf(linea, "ignorar %d %d %d %d", &x0, &y0, &x1, &y1) == 4) {
                ign[*nign][0] = x0; ign[*nign][1] = y0; ign[*nign][2] = x1; ign[*nign][3] = y1; (*nign)++;
            }
        }
    }
    fclose(f);
    if (n < 4) { ESP_LOGE(TAG, "calibracion.txt necesita 4 líneas 'u v X Y' (hay %d)", n); return false; }
    if (!hom_resolver(img, mun, 4, H)) { ESP_LOGE(TAG, "los 4 puntos de calibración no sirven (¿hay 3 alineados?)"); return false; }
    ESP_LOGI(TAG, "homografía: [%.5f %.5f %.5f | %.5f %.5f %.5f | %.5f %.5f %.5f]",
             H->h[0], H->h[1], H->h[2], H->h[3], H->h[4], H->h[5], H->h[6], H->h[7], H->h[8]);
    return true;
}

void vivo_ejecutar(int duracion_s)
{
    mkdir("/sdcard/salida", 0777);
    mkdir("/sdcard/mov", 0777);
    Homografia H; double ymin = 0, ymax = 0; bool hay_zona = false;
    double puntos_img[4][2]; int ign[VIVO_MAX_IGN][4], nign = 0;
    if (!leer_calibracion("/sdcard/calibracion.txt", &H, &ymin, &ymax, &hay_zona, puntos_img, ign, &nign)) return;
    if (nign > 0) ESP_LOGI(TAG, "zonas ignoradas: %d", nign);
    if (!cad_iniciar(&C, H, "/sdcard/salida", "/sdcard/hora.txt", 0.0)) {
        ESP_LOGE(TAG, "no se pudo leer /sdcard/hora.txt: error de arranque");
        return;
    }
    if (hay_zona) { cad_zona(&C, ymin, ymax); ESP_LOGI(TAG, "zona de medición: Y de %.1f a %.1f m", ymin, ymax); }
    if (camera_init() != ESP_OK) { ESP_LOGE(TAG, "no inicia la cámara"); return; }
    if (!md_init()) { ESP_LOGE(TAG, "sin memoria para el detector"); return; }

    DetReg *reg = (DetReg *)heap_caps_malloc(VIVO_MAX_REG * sizeof(DetReg), MALLOC_CAP_SPIRAM);
    if (!reg) { ESP_LOGE(TAG, "sin memoria para el registro"); return; }
    int nreg = 0;

    // Calentamiento de la cámara (~4 s): exposición y balance de blancos
    int64_t t0 = esp_timer_get_time();
    while (esp_timer_get_time() - t0 < 4000000) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }
    {   // Imagen del encuadre inicial, con los puntos de calibración marcados
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            bool ok = guardar_encuadre("/sdcard/encuadre.bmp", fb->buf, fb->width, fb->height, puntos_img, 4, ign, nign);
            ESP_LOGI(TAG, "encuadre inicial %s: /sdcard/encuadre.bmp", ok ? "guardado" : "NO se pudo guardar");
            esp_camera_fb_return(fb);
        }
    }
    ESP_LOGW(TAG, ">>> Midiendo %d s. El primer cuadro es el fondo: no debe haber vehículos en la escena al empezar.", duracion_s);

    int64_t t_ini = esp_timer_get_time();
    int cuadro = 0, con_det = 0, nimg = 0;
    double t_ult_img = -1e9;
    int64_t suma_md = 0, max_md = 0, suma_cad = 0, max_cad = 0;
    for (;;) {
        double seg = (esp_timer_get_time() - t_ini) / 1e6;
        if (seg >= duracion_s) break;
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) continue;
        double t = (esp_timer_get_time() - t_ini) / 1e6;
        Blob b[MD_MAX_BLOBS];
        MotionStats st = {};
        int64_t a = esp_timer_get_time();
        int n = md_process(fb->buf, fb->width, fb->height, b, MD_MAX_BLOBS, &st);
        int64_t dmd = esp_timer_get_time() - a;
        if (n > 0 && nimg < VIVO_MAX_IMG && t - t_ult_img >= VIVO_SEP_IMG_S) {   // revisión del detector (solo desarrollo)
            char ruta[40];
            snprintf(ruta, sizeof ruta, "/sdcard/mov/v_%03d.bmp", nimg);
            if (guardar_debug_bmp(ruta, md_gris(), md_mascara(), b, n)) { nimg++; t_ult_img = t; }
        }
        esp_camera_fb_return(fb);
        if (n < 0) { ESP_LOGE(TAG, "la imagen debe ser de 320x240 en RGB565"); break; }

        double det[MD_MAX_BLOBS][2];
        int nd = 0;
        for (int i = 0; i < n; i++) {
            bool ignorada = false;                                  // punto de contacto dentro de una zona ignorada
            for (int k = 0; k < nign; k++)
                if (b[i].gx >= ign[k][0] && b[i].gx <= ign[k][2] && b[i].gy >= ign[k][1] && b[i].gy <= ign[k][3]) ignorada = true;
            if (ignorada) continue;
            det[nd][0] = b[i].gx; det[nd][1] = b[i].gy; nd++;
            if (nreg < VIVO_MAX_REG) { reg[nreg].t = (float)t; reg[nreg].gx = b[i].gx; reg[nreg].gy = b[i].gy; reg[nreg].cuadro = cuadro; nreg++; }
        }
        if (nd > 0) con_det++;
        Resultado r[TRK_MAX_PISTAS];
        int64_t c0 = esp_timer_get_time();
        int nr = cad_cuadro(&C, t, det, nd, r);
        int64_t dcad = esp_timer_get_time() - c0;
        for (int i = 0; i < nr; i++)
            ESP_LOGI(TAG, "vehículo %d: %.1f km/h, carril X=%.2f m, %d muestras", r[i].id, r[i].kmh, r[i].x_medio, r[i].muestras);

        suma_md += dmd; suma_cad += dcad;
        if (dmd > max_md) max_md = dmd;
        if (dcad > max_cad) max_cad = dcad;
        cuadro++;
    }
    {   // Imagen final del encuadre, para comprobar que la cámara no se movió
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) { guardar_encuadre("/sdcard/encuadre_fin.bmp", fb->buf, fb->width, fb->height, puntos_img, 4, ign, nign); esp_camera_fb_return(fb); }
    }
    int nr_fin;
    { Resultado r[TRK_MAX_PISTAS]; nr_fin = cad_cerrar(&C, r);
      for (int i = 0; i < nr_fin; i++) ESP_LOGI(TAG, "vehículo %d (al cerrar): %.1f km/h, carril X=%.2f m", r[i].id, r[i].kmh, r[i].x_medio); }

    FILE *g = fopen("/sdcard/det_vivo.csv", "w");
    if (g) {
        fprintf(g, "cuadro,t_s,gx,gy\n");
        for (int i = 0; i < nreg; i++) fprintf(g, "%d,%.4f,%.2f,%.2f\n", reg[i].cuadro, reg[i].t, reg[i].gx, reg[i].gy);
        fclose(g);
    }
    free(reg);

    double fps = cuadro / (double)duracion_s;
    ESP_LOGI(TAG, "cuadros: %d (%.1f FPS), con detecciones: %d, detecciones crudas guardadas: %d%s, imágenes de depuración: %d",
             cuadro, fps, con_det, nreg, nreg >= VIVO_MAX_REG ? " (LLENO)" : "", nimg);
    ESP_LOGI(TAG, "detector: medio %lld us, máximo %lld us | cadena: medio %lld us, máximo %lld us",
             cuadro ? (long long)(suma_md / cuadro) : 0LL, (long long)max_md, cuadro ? (long long)(suma_cad / cuadro) : 0LL, (long long)max_cad);
    ESP_LOGI(TAG, "vehículos registrados: %d, errores de escritura: %d", C.registrados, C.errores);
}
