#include <stdio.h>
#include <math.h>
#include "homografia.h"

int main()
{
    // Puntos de calibración (los mismos de calibracion_sim.json)
    const double img[4][2] = {{186, 112}, {309, 112}, {225, 48}, {171, 48}};
    const double mun[4][2] = {{0, 6}, {7, 6}, {7, 30}, {0, 30}};

    Homografia H;
    if (!hom_resolver(img, mun, 4, &H)) { printf("FALLO: no se pudo resolver H\n"); return 1; }
    printf("H = [%.6f %.6f %.6f; %.6f %.6f %.6f; %.6f %.6f %.6f]\n",
           H.h[0], H.h[1], H.h[2], H.h[3], H.h[4], H.h[5], H.h[6], H.h[7], H.h[8]);

    int fallos = 0;
    printf("\n1) Los 4 puntos de calibración (error esperado: casi 0)\n");
    for (int i = 0; i < 4; i++) {
        double X, Y;
        hom_aplicar(H, img[i][0], img[i][1], &X, &Y);
        double e = hypot(X - mun[i][0], Y - mun[i][1]);
        printf("   (%.0f, %.0f) -> (%.3f, %.3f)  esperado (%.1f, %.1f)  error %.2e m  %s\n",
               img[i][0], img[i][1], X, Y, mun[i][0], mun[i][1], e, e < 1e-6 ? "OK" : "FALLA");
        if (e >= 1e-6) fallos++;
    }

    printf("\n2) Puntos intermedios de la calzada (el error viene de que los puntos de calibración se redondearon a píxel)\n");
    const struct { double u, v, X, Y; } pr[] = {
        {203.055, 84.520, 1.75, 12.0}, {219.961, 56.277, 5.25, 24.0}, {213.175, 67.613, 3.5, 18.0},
        {175.952, 67.613, 0.0, 18.0}, {272.604, 84.520, 7.0, 12.0}, {186.013, 50.589, 1.75, 28.0},
    };
    for (auto &p : pr) {
        double X, Y;
        hom_aplicar(H, p.u, p.v, &X, &Y);
        double e = hypot(X - p.X, Y - p.Y);
        printf("   (%.3f, %.3f) -> (%.3f, %.3f)  esperado (%.2f, %.2f)  error %.3f m  %s\n",
               p.u, p.v, X, Y, p.X, p.Y, e, e < 0.4 ? "OK" : "FALLA");
        if (e >= 0.4) fallos++;
    }

    printf("\n3) Puntos alineados (no deben dar una homografía)\n");
    const double mal[4][2] = {{10, 10}, {20, 20}, {30, 30}, {40, 40}};
    Homografia H2;
    bool ok = hom_resolver(mal, mun, 4, &H2);
    printf("   hom_resolver devolvió %s  %s\n", ok ? "true" : "false", ok ? "FALLA" : "OK");
    if (ok) fallos++;

    printf("\n%s\n", fallos == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY PRUEBAS FALLIDAS");
    return fallos ? 1 : 0;
}
