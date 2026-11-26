// Hito 7, paso B: decodificación de YOLO-Fastest 1.1 (COCO, INT8, 320x320) sin dependencias de TFLite.
// Entrada del modelo: 320x320x3 RGB, int8 = pixel - 128. Una imagen de 320x240 se rellena con 40 filas grises arriba y abajo.
// Salidas: cabeza 0 = 10x10x255 (anclas 3,4,5), cabeza 1 = 20x20x255 (anclas 0,1,2); 255 = 3 anclas x (x,y,w,h,obj,80 clases).
#pragma once
#include <stdint.h>

#define YOLO_TAM        320
#define YOLO_CLASES     80
#define YOLO_MAX_CAJAS  32          // cajas que devuelve tras NMS

enum YoloClase { YOLO_CAR = 2, YOLO_MOTO = 3, YOLO_BUS = 5, YOLO_CAMION = 7 };

struct YoloCaja {
    float x0, y0, x1, y1;           // en píxeles del lienzo 320x320 (usar yolo_a_imagen para volver a la imagen original)
    float conf;                     // obj * clase
    int   clase;                    // índice COCO
};

// Parámetros de cuantización de una salida: real = (q - cero) * escala
struct YoloSalida { const int8_t *datos; float escala; int cero; };

// Decodifica las dos cabezas. cab10 = salida 10x10x255, cab20 = salida 20x20x255.
// Solo devuelve las clases de 'solo_vehiculos' (car, moto, bus, camión) si es true. Devuelve cuántas cajas escribió en out.
int yolo_decodificar(const YoloSalida &cab10, const YoloSalida &cab20, float umbral, bool solo_vehiculos,
                     YoloCaja *out, int max_out);

// Prepara la entrada int8 (320x320x3) desde una imagen RGB888 (w x h, w <= 320, h <= 320), centrada con relleno gris.
// Devuelve el desplazamiento (ox, oy) con que quedó pegada; escala 1 (no se redimensiona). Para imágenes 320x240: oy = 40.
void yolo_entrada_rgb888(const uint8_t *rgb, int w, int h, int8_t *entrada, int *ox, int *oy);

// Igual, desde RGB565 (como entrega la cámara). Si 'bytes_invertidos' es true, el byte alto va primero en memoria.
void yolo_entrada_rgb565(const uint8_t *rgb565, int w, int h, bool bytes_invertidos, int8_t *entrada, int *ox, int *oy);
