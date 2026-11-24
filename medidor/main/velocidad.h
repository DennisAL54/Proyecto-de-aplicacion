#pragma once
#define VEL_MAX_MUESTRAS 64
struct Muestra { double t, x, y; };
bool vel_ajustar(const Muestra *m, int n, double *kmh, int *usadas);
