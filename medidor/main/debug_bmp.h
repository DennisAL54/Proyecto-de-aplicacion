#pragma once
#include "motion.h"

// Guarda un BMP de 320x120 para revisar el detector en la computadora:
//   izquierda: imagen reducida con las cajas (rojo) y el punto de contacto con el suelo (verde)
//   derecha:   máscara de movimiento (blanco = cambió)
// Solo para desarrollo: el firmware final no guarda imágenes (REQ-SEG-002).
bool guardar_debug_bmp(const char *ruta, const uint8_t *gris, const uint8_t *mascara, const Blob *b, int nb);
