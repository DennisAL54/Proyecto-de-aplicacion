#include "debug_bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

static void pintar(uint8_t *fila, int x, int ancho, uint8_t b, uint8_t g, uint8_t r)
{
    if (x < 0 || x >= ancho) return;
    fila[3 * x] = b; fila[3 * x + 1] = g; fila[3 * x + 2] = r;       // BMP guarda B, G, R
}

bool guardar_debug_bmp(const char *ruta, const uint8_t *gris, const uint8_t *mascara, const Blob *b, int nb)
{
    const int W = 2 * MD_W, H = MD_H;                                 // 320 x 120 (fila de 960 bytes, múltiplo de 4)
    const uint32_t datos = (uint32_t)W * H * 3;
    uint8_t cab[54] = {};
    cab[0] = 'B'; cab[1] = 'M';
    put32(cab + 2, 54 + datos); put32(cab + 10, 54); put32(cab + 14, 40);
    put32(cab + 18, W); put32(cab + 22, H); put16(cab + 26, 1); put16(cab + 28, 24); put32(cab + 34, datos);

    FILE *f = fopen(ruta, "wb");
    if (!f) return false;
    fwrite(cab, 1, 54, f);
    uint8_t *fila = (uint8_t *)malloc(W * 3);
    if (!fila) { fclose(f); return false; }

    for (int y = H - 1; y >= 0; y--) {                                // el BMP va de abajo hacia arriba
        for (int x = 0; x < MD_W; x++) {
            uint8_t g = gris[y * MD_W + x];
            uint8_t m = mascara[y * MD_W + x] ? 255 : 0;
            pintar(fila, x, W, g, g, g);
            pintar(fila, x + MD_W, W, m, m, m);
        }
        for (int i = 0; i < nb; i++) {
            const Blob &k = b[i];
            if (y >= k.y && y < k.y + k.h) {
                bool borde_h = (y == k.y || y == k.y + k.h - 1);
                for (int x = k.x; x < k.x + k.w; x++)
                    if (borde_h || x == k.x || x == k.x + k.w - 1) {
                        pintar(fila, x, W, 0, 0, 255);                 // caja en rojo
                        pintar(fila, x + MD_W, W, 0, 0, 255);
                    }
            }
            int gx = (int)(k.gx / 2.0f), gy = (int)(k.gy / 2.0f);     // punto de contacto en verde (3x3)
            if (abs(y - gy) <= 1)
                for (int x = gx - 1; x <= gx + 1; x++) pintar(fila, x, W, 0, 255, 0);
        }
        fwrite(fila, 1, W * 3, f);
    }
    free(fila);
    fflush(f);
    fclose(f);
    return true;
}
