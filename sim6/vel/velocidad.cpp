#include "velocidad.h"
#include <math.h>

// Pendiente y ordenada de la recta v = a*t + b por mínimos cuadrados, solo con las muestras marcadas.
static bool recta(const double *t, const double *v, const bool *usar, int n, double *a, double *b)
{
    double st = 0, sv = 0, stt = 0, stv = 0;
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (!usar[i]) continue;
        st += t[i]; sv += v[i]; stt += t[i] * t[i]; stv += t[i] * v[i];
        k++;
    }
    double den = k * stt - st * st;
    if (k < 2 || fabs(den) < 1e-12) return false;
    *a = (k * stv - st * sv) / den;
    *b = (sv - *a * st) / k;
    return true;
}

bool vel_ajustar(const Muestra *m, int n, double *kmh, int *usadas)
{
    if (n < 2 || n > VEL_MAX_MUESTRAS) return false;
    double t[VEL_MAX_MUESTRAS], x[VEL_MAX_MUESTRAS], y[VEL_MAX_MUESTRAS];
    bool usar[VEL_MAX_MUESTRAS];
    for (int i = 0; i < n; i++) {                       // el tiempo se mide desde la primera muestra
        t[i] = m[i].t - m[0].t; x[i] = m[i].x; y[i] = m[i].y; usar[i] = true;
    }
    int quedan = n;
    double mx = 0, cx = 0, my = 0, cy = 0;
    for (int pasada = 0; pasada < 3; pasada++) {
        if (!recta(t, x, usar, n, &mx, &cx) || !recta(t, y, usar, n, &my, &cy)) return false;
        if (pasada == 2 || n < 5) break;                // el rechazo de atípicos necesita al menos 5 muestras
        double res[VEL_MAX_MUESTRAS], suma = 0, suma2 = 0;
        for (int i = 0; i < n; i++) {
            res[i] = hypot(x[i] - (mx * t[i] + cx), y[i] - (my * t[i] + cy));
            if (usar[i]) { suma += res[i]; suma2 += res[i] * res[i]; }
        }
        double media = suma / quedan;
        double sigma = sqrt(fmax(0.0, suma2 / quedan - media * media));
        double lim = fmax(0.8, 2.5 * sigma);
        int nuevos = 0;
        bool nuevo[VEL_MAX_MUESTRAS];
        for (int i = 0; i < n; i++) { nuevo[i] = res[i] < lim; nuevos += nuevo[i]; }
        if (nuevos < 4 || nuevos == quedan) break;      // no se descarta nada nuevo, o quedarían muy pocas
        for (int i = 0; i < n; i++) usar[i] = nuevo[i];
        quedan = nuevos;
    }
    *kmh = hypot(mx, my) * 3.6;
    if (usadas) *usadas = quedan;
    return true;
}
