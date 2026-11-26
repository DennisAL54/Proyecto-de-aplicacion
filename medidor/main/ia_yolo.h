#pragma once
// Hito 7, paso C: YOLO-Fastest INT8 con TFLite Micro en la ESP32-S3.
// Lee un BMP 24 bits (<= 320x320) de la SD, lo pasa por el modelo y escribe en el log las cajas de vehículos.
#include <stddef.h>
#include <stdbool.h>

// Prepara el intérprete (arena en PSRAM). Devuelve false si falla. Se llama una vez.
bool yolo_iniciar(size_t arena_bytes);
// Procesa un BMP; umbral típico 0.15 para comparar con el PC. Devuelve el número de cajas, o -1 si falla.
int  yolo_probar_bmp(const char *ruta, float umbral);
// Libera todo.
void yolo_terminar(void);
