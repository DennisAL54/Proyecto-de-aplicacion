#pragma once

// Homografía: convierte puntos de la imagen (píxeles) en puntos del suelo (metros).
struct Homografia {
    double h[9];     // matriz 3x3 por filas, con h[8] = 1
};

// Calcula H a partir de n >= 4 pares de puntos: img[i] = {u, v} en píxeles, mun[i] = {X, Y} en metros.
// Con más de 4 puntos usa mínimos cuadrados. Devuelve false si los puntos no sirven (por ejemplo, alineados).
bool hom_resolver(const double (*img)[2], const double (*mun)[2], int n, Homografia *H);

// Convierte el píxel (u, v) en metros (X, Y).
void hom_aplicar(const Homografia &H, double u, double v, double *X, double *Y);
