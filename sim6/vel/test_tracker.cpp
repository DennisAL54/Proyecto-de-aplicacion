#include <stdio.h>
#include <math.h>
#include <random>
#include "tracker.h"

static int fallos = 0;
static void verificar(const char *n, bool ok, const char *d) { printf("   %-58s %s  %s\n", n, ok ? "OK   " : "FALLA", d); if (!ok) fallos++; }

struct Veh { double x0, y0, kmh, t0, t1; int sentido; };   // sentido +1: Y crece; -1: Y decrece

int main()
{
    const double FPS = 7.0, DT = 1.0 / FPS;
    std::mt19937 gen(3);
    std::normal_distribution<double> ruido(0.0, 0.10);
    char buf[160];

    // ---- Prueba 1: un vehículo a 40 km/h
    {
        printf("1) Un vehículo a 40 km/h, carril X = 1.75 m\n");
        Tracker T; trk_iniciar(&T, trk_config_defecto());
        Resultado r[TRK_MAX_PISTAS]; int nres = 0; Resultado todos[16];
        double v = 40 / 3.6;
        for (int k = 0; k < 60; k++) {
            double t = k * DT, y = v * t;
            double det[1][2]; int nd = 0;
            if (y <= 24) { det[0][0] = 1.75 + ruido(gen); det[0][1] = y + ruido(gen); nd = 1; }
            int n = trk_actualizar(&T, t, det, nd, r);
            for (int i = 0; i < n; i++) todos[nres++] = r[i];
        }
        int n = trk_cerrar_todo(&T, r); for (int i = 0; i < n; i++) todos[nres++] = r[i];
        snprintf(buf, sizeof buf, "resultados = %d", nres);
        verificar("se genera exactamente un resultado", nres == 1, buf);
        if (nres >= 1) { snprintf(buf, sizeof buf, "calculada %.2f km/h", todos[0].kmh); verificar("velocidad dentro de ±2 km/h de 40", fabs(todos[0].kmh - 40) < 2, buf); }
    }

    // ---- Prueba 2: dos vehículos en carriles opuestos y velocidades distintas, simultáneos
    {
        printf("2) Dos vehículos a la vez: 35 km/h (Y crece) y 55 km/h (Y decrece)\n");
        Tracker T; trk_iniciar(&T, trk_config_defecto());
        Resultado r[TRK_MAX_PISTAS]; Resultado todos[16]; int nres = 0;
        double va = 35 / 3.6, vb = 55 / 3.6;
        for (int k = 0; k < 80; k++) {
            double t = k * DT, ya = va * t, yb = 24 - vb * t;
            double det[2][2]; int nd = 0;
            if (ya <= 24) { det[nd][0] = 1.75 + ruido(gen); det[nd][1] = ya + ruido(gen); nd++; }
            if (yb >= 0) { det[nd][0] = 5.25 + ruido(gen); det[nd][1] = yb + ruido(gen); nd++; }
            int n = trk_actualizar(&T, t, det, nd, r);
            for (int i = 0; i < n; i++) todos[nres++] = r[i];
        }
        int n = trk_cerrar_todo(&T, r); for (int i = 0; i < n; i++) todos[nres++] = r[i];
        snprintf(buf, sizeof buf, "resultados = %d", nres);
        verificar("se generan dos resultados", nres == 2, buf);
        bool a = false, b = false;
        for (int i = 0; i < nres; i++) { if (fabs(todos[i].kmh - 35) < 2) a = true; if (fabs(todos[i].kmh - 55) < 2) b = true; }
        verificar("una pista da 35 ±2 y la otra 55 ±2", a && b, "");
    }

    // ---- Prueba 3: un vehículo que se pierde 2 cuadros en la mitad del recorrido
    {
        printf("3) Faltan 2 detecciones seguidas a mitad de recorrido (no debe partir la pista)\n");
        Tracker T; trk_iniciar(&T, trk_config_defecto());
        Resultado r[TRK_MAX_PISTAS]; Resultado todos[16]; int nres = 0;
        double v = 50 / 3.6;
        for (int k = 0; k < 60; k++) {
            double t = k * DT, y = v * t;
            double det[1][2]; int nd = 0;
            if (y <= 24 && !(k == 6 || k == 7)) { det[0][0] = 1.75 + ruido(gen); det[0][1] = y + ruido(gen); nd = 1; }
            int n = trk_actualizar(&T, t, det, nd, r);
            for (int i = 0; i < n; i++) todos[nres++] = r[i];
        }
        int n = trk_cerrar_todo(&T, r); for (int i = 0; i < n; i++) todos[nres++] = r[i];
        snprintf(buf, sizeof buf, "resultados = %d", nres);
        verificar("sigue siendo un solo resultado", nres == 1, buf);
        if (nres >= 1) { snprintf(buf, sizeof buf, "calculada %.2f km/h", todos[0].kmh); verificar("velocidad dentro de ±2 km/h de 50", fabs(todos[0].kmh - 50) < 2, buf); }
    }

    // ---- Prueba 4: ruido espurio de una sola detección (no debe dar velocidad)
    {
        printf("4) Una detección falsa aislada\n");
        Tracker T; trk_iniciar(&T, trk_config_defecto());
        Resultado r[TRK_MAX_PISTAS]; int nres = 0;
        for (int k = 0; k < 20; k++) {
            double det[1][2] = {{3.0, 10.0}}; int nd = (k == 5) ? 1 : 0;
            nres += trk_actualizar(&T, k * DT, det, nd, r);
        }
        nres += trk_cerrar_todo(&T, r);
        snprintf(buf, sizeof buf, "resultados = %d", nres);
        verificar("no se genera ningún resultado", nres == 0, buf);
    }

    // ---- Prueba 5: más de 4 vehículos a la vez (el quinto se ignora, nada se rompe)
    {
        printf("5) Cinco vehículos a la vez (el límite es 4)\n");
        Tracker T; trk_iniciar(&T, trk_config_defecto());
        Resultado r[TRK_MAX_PISTAS]; Resultado todos[32]; int nres = 0;
        double v = 40 / 3.6;
        for (int k = 0; k < 60; k++) {
            double t = k * DT, y = v * t; double det[5][2]; int nd = 0;
            if (y <= 24) for (int c = 0; c < 5; c++) { det[nd][0] = 1.0 + 2.2 * c; det[nd][1] = y; nd++; }
            int n = trk_actualizar(&T, t, det, nd, r);
            for (int i = 0; i < n; i++) todos[nres++] = r[i];
        }
        int n = trk_cerrar_todo(&T, r); for (int i = 0; i < n; i++) todos[nres++] = r[i];
        snprintf(buf, sizeof buf, "resultados = %d", nres);
        verificar("se registran exactamente 4", nres == 4, buf);
    }

    printf("\n%s\n", fallos == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY PRUEBAS QUE FALLARON");
    return fallos ? 1 : 0;
}
