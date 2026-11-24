#include "homografia.h"
#include <math.h>

// Resuelve el sistema M x = b de 8x8 por eliminación gaussiana con pivoteo parcial.
static bool resolver8(double M[8][9])
{
    for (int c = 0; c < 8; c++) {
        int p = c;
        for (int r = c + 1; r < 8; r++)
            if (fabs(M[r][c]) > fabs(M[p][c])) p = r;
        if (fabs(M[p][c]) < 1e-12) return false;
        if (p != c)
            for (int k = 0; k < 9; k++) { double t = M[c][k]; M[c][k] = M[p][k]; M[p][k] = t; }
        for (int r = 0; r < 8; r++) {
            if (r == c) continue;
            double f = M[r][c] / M[c][c];
            for (int k = c; k < 9; k++) M[r][k] -= f * M[c][k];
        }
    }
    for (int r = 0; r < 8; r++) M[r][8] /= M[r][r];
    return true;
}

bool hom_resolver(const double (*img)[2], const double (*mun)[2], int n, Homografia *H)
{
    if (n < 4) return false;
    // Se normalizan los puntos (centrados y con escala parecida) para que la matriz esté bien condicionada.
    double cu = 0, cv = 0, cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { cu += img[i][0]; cv += img[i][1]; cx += mun[i][0]; cy += mun[i][1]; }
    cu /= n; cv /= n; cx /= n; cy /= n;
    double su = 0, sx = 0;
    for (int i = 0; i < n; i++) {
        su += fabs(img[i][0] - cu) + fabs(img[i][1] - cv);
        sx += fabs(mun[i][0] - cx) + fabs(mun[i][1] - cy);
    }
    su /= (2.0 * n); sx /= (2.0 * n);
    if (su < 1e-9 || sx < 1e-9) return false;

    // Ecuaciones normales (A^T A) h = A^T b, con A de 2n x 8, para los puntos normalizados.
    double M[8][9] = {};
    for (int i = 0; i < n; i++) {
        double u = (img[i][0] - cu) / su, v = (img[i][1] - cv) / su;
        double X = (mun[i][0] - cx) / sx, Y = (mun[i][1] - cy) / sx;
        double f1[8] = {u, v, 1, 0, 0, 0, -u * X, -v * X};
        double f2[8] = {0, 0, 0, u, v, 1, -u * Y, -v * Y};
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) M[r][c] += f1[r] * f1[c] + f2[r] * f2[c];
            M[r][8] += f1[r] * X + f2[r] * Y;
        }
    }
    if (!resolver8(M)) return false;
    double g[9];
    for (int i = 0; i < 8; i++) g[i] = M[i][8];
    g[8] = 1.0;

    // Deshace la normalización: H = Tm^-1 * G * Ti, con Ti: pixel -> normalizado y Tm: metros -> normalizado.
    double Ti[9] = {1 / su, 0, -cu / su, 0, 1 / su, -cv / su, 0, 0, 1};
    double GT[9];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            GT[r * 3 + c] = 0;
            for (int k = 0; k < 3; k++) GT[r * 3 + c] += g[r * 3 + k] * Ti[k * 3 + c];
        }
    double Tmi[9] = {sx, 0, cx, 0, sx, cy, 0, 0, 1};                  // inversa de Tm
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            H->h[r * 3 + c] = 0;
            for (int k = 0; k < 3; k++) H->h[r * 3 + c] += Tmi[r * 3 + k] * GT[k * 3 + c];
        }
    if (fabs(H->h[8]) < 1e-12) return false;
    for (int i = 0; i < 9; i++) H->h[i] /= H->h[8];
    return true;
}

void hom_aplicar(const Homografia &H, double u, double v, double *X, double *Y)
{
    double w = H.h[6] * u + H.h[7] * v + H.h[8];
    *X = (H.h[0] * u + H.h[1] * v + H.h[2]) / w;
    *Y = (H.h[3] * u + H.h[4] * v + H.h[5]) / w;
}
