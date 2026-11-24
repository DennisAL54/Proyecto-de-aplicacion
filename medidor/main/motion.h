#pragma once
#include <stdint.h>

// Detección de movimiento sobre imágenes RGB565 de 320x240 (se reduce a 160x120 en escala de grises).
#define MD_W 160
#define MD_H 120
#define MD_MAX_BLOBS 8

struct Blob {
    int x, y, w, h;      // caja en la imagen reducida (160x120)
    int area;            // píxeles de movimiento dentro de la caja
    float gx, gy;        // punto de contacto con el suelo, en píxeles de la imagen original (320x240)
};

struct MotionStats {
    int frame;           // número de cuadro procesado
    int desfase;         // cambio global de brillo estimado (niveles de gris)
    int px_mov;          // píxeles de movimiento después de la limpieza
    bool cambio_global;  // true si se descartó el cuadro por un cambio brusco de luz
};

bool md_init();
// Devuelve el número de manchas (vehículos candidatos), o -1 si la imagen no es de 320x240.
int md_process(const uint8_t *rgb565, int ancho, int alto, Blob *out, int max_out, MotionStats *st);
const uint8_t *md_gris();      // última imagen reducida (para depuración)
const uint8_t *md_mascara();   // última máscara de movimiento (0 o 1)
