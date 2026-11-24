#pragma once

#define VEL_MAX_MUESTRAS 64

// Una muestra del seguimiento: instante (s) y posición sobre el suelo (m).
struct Muestra {
    double t, x, y;
};

// Velocidad como pendiente del ajuste lineal posición-tiempo de toda la trayectoria.
// Ajusta x(t) e y(t) por mínimos cuadrados y devuelve hypot(vx, vy) en km/h.
// Con 5 o más muestras descarta los valores atípicos (residuo mayor que max(0.8 m, 2.5 desviaciones)).
// usadas recibe cuántas muestras se usaron en el ajuste final (puede ser nullptr).
// Devuelve false si hay menos de 2 muestras, más de VEL_MAX_MUESTRAS o el tiempo no varía.
bool vel_ajustar(const Muestra *m, int n, double *kmh, int *usadas);
