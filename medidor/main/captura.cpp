#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "img_converters.h"
#include "captura.h"

// Si el BMP sale con colores raros pero el JPEG sale bien, ponga 1 aquí.
#define RGB565_INVERTIDO 0

static inline void rgb565_a_rgb(const uint8_t *p, uint8_t &r, uint8_t &g, uint8_t &b)
{
#if RGB565_INVERTIDO
    uint16_t v = (p[1] << 8) | p[0];
#else
    uint16_t v = (p[0] << 8) | p[1];
#endif
    r = ((v >> 11) & 0x1F) << 3;
    g = ((v >> 5)  & 0x3F) << 2;
    b = ( v        & 0x1F) << 3;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

bool guardar_bmp(const camera_fb_t *fb, const char *ruta)
{
    const int W = fb->width, H = fb->height;
    const int fila = (W * 3 + 3) & ~3;            // cada fila se rellena a múltiplo de 4 bytes
    const uint32_t datos = (uint32_t)fila * H;

    uint8_t cab[54] = {};
    cab[0] = 'B'; cab[1] = 'M';
    put32(cab + 2, 54 + datos);                    // tamaño del archivo
    put32(cab + 10, 54);                           // dónde empiezan los píxeles
    put32(cab + 14, 40);                           // tamaño del encabezado DIB
    put32(cab + 18, W);
    put32(cab + 22, H);                            // positivo = filas de abajo hacia arriba
    put16(cab + 26, 1);
    put16(cab + 28, 24);                           // 24 bits por píxel
    put32(cab + 34, datos);

    FILE *f = fopen(ruta, "wb");
    if (!f) return false;
    fwrite(cab, 1, 54, f);

    uint8_t *linea = (uint8_t *)heap_caps_malloc(fila, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!linea) { fclose(f); return false; }
    for (int y = H - 1; y >= 0; y--) {
        memset(linea, 0, fila);
        for (int x = 0; x < W; x++) {
            uint8_t r, g, b;
            rgb565_a_rgb(fb->buf + 2 * (y * W + x), r, g, b);
            linea[3 * x] = b; linea[3 * x + 1] = g; linea[3 * x + 2] = r;   // BMP guarda B, G, R
        }
        fwrite(linea, 1, fila, f);
    }
    free(linea);
    fflush(f); fsync(fileno(f)); fclose(f);
    return true;
}

bool guardar_jpeg(camera_fb_t *fb, const char *ruta)
{
    uint8_t *jpg = nullptr;
    size_t len = 0;
    if (!frame2jpg(fb, 85, &jpg, &len)) return false;
    FILE *f = fopen(ruta, "wb");
    bool ok = f && (fwrite(jpg, 1, len, f) == len);
    if (f) { fflush(f); fsync(fileno(f)); fclose(f); }
    free(jpg);
    return ok;
}
