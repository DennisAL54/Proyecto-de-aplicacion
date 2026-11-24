#include "tracker.h"
#include <math.h>

TrkConfig trk_config_defecto()
{
    TrkConfig c;
    c.compuerta_m = 3.0; c.lateral_m = 1.0; c.max_perdidos = 3; c.min_muestras = 5; c.min_recorrido_m = 5.0;
    return c;
}

void trk_iniciar(Tracker *T, const TrkConfig &cfg)
{
    T->cfg = cfg; T->siguiente_id = 1;
    for (int i = 0; i < TRK_MAX_PISTAS; i++) { T->pistas[i].activa = false; T->pistas[i].n = 0; }
}

// Posición esperada de la pista en el instante t. La velocidad sale de un ajuste lineal sobre las últimas
// TRK_VENTANA muestras: es más estable que usar solo las dos últimas cuando la posición tiene ruido.
#define TRK_VENTANA 5
static void predecir(const Pista &p, double t, double *x, double *y)
{
    const Muestra &u = p.m[p.n - 1];
    *x = u.x; *y = u.y;
    int k = p.n < TRK_VENTANA ? p.n : TRK_VENTANA;
    if (k < 2) return;
    const Muestra *m = &p.m[p.n - k];
    double mt = 0, mx = 0, my = 0;
    for (int i = 0; i < k; i++) { mt += m[i].t; mx += m[i].x; my += m[i].y; }
    mt /= k; mx /= k; my /= k;
    double stt = 0, stx = 0, sty = 0;
    for (int i = 0; i < k; i++) {
        double dt = m[i].t - mt;
        stt += dt * dt; stx += dt * (m[i].x - mx); sty += dt * (m[i].y - my);
    }
    if (stt < 1e-12) return;
    double vx = stx / stt, vy = sty / stt;
    // La recta ajustada pasa por la media; se evalúa en t.
    *x = mx + vx * (t - mt);
    *y = my + vy * (t - mt);
}

// Cierra una pista: si recorrió lo suficiente, calcula su velocidad y la agrega a out.
static int cerrar(Tracker *T, Pista &p, Resultado *out, int nout)
{
    p.activa = false;
    if (p.n < T->cfg.min_muestras) return nout;
    double rec = hypot(p.m[p.n - 1].x - p.m[0].x, p.m[p.n - 1].y - p.m[0].y);
    if (rec < T->cfg.min_recorrido_m) return nout;
    double kmh; int usadas;
    if (!vel_ajustar(p.m, p.n, &kmh, &usadas)) return nout;
    out[nout].id = p.id; out[nout].kmh = kmh; out[nout].muestras = usadas;
    out[nout].t_inicio = p.m[0].t; out[nout].t_fin = p.m[p.n - 1].t;
    double sx = 0; for (int i = 0; i < p.n; i++) sx += p.m[i].x;
    out[nout].x_medio = sx / p.n;
    return nout + 1;
}

int trk_actualizar(Tracker *T, double t, const double (*det)[2], int nd, Resultado *out)
{
    const TrkConfig &c = T->cfg;
    int nout = 0;
    bool det_usada[16] = {};
    if (nd > 16) nd = 16;
    bool pista_asignada[TRK_MAX_PISTAS] = {};

    // Asignación voraz: se toma siempre el par pista-detección más cercano que cumpla la compuerta.
    for (;;) {
        double mejor = 1e18; int bp = -1, bd = -1;
        for (int i = 0; i < TRK_MAX_PISTAS; i++) {
            if (!T->pistas[i].activa || pista_asignada[i]) continue;
            double px, py; predecir(T->pistas[i], t, &px, &py);
            for (int j = 0; j < nd; j++) {
                if (det_usada[j]) continue;
                double dx = det[j][0] - px, dy = det[j][1] - py;
                double d = hypot(dx, dy);
                if (d > c.compuerta_m || fabs(dx) > c.lateral_m) continue;
                if (d < mejor) { mejor = d; bp = i; bd = j; }
            }
        }
        if (bp < 0) break;
        Pista &p = T->pistas[bp];
        if (p.n < VEL_MAX_MUESTRAS) { p.m[p.n].t = t; p.m[p.n].x = det[bd][0]; p.m[p.n].y = det[bd][1]; p.n++; }
        p.perdidos = 0; pista_asignada[bp] = true; det_usada[bd] = true;
    }

    // Pistas sin detección: se cuentan los cuadros perdidos y se cierran al pasar el límite.
    for (int i = 0; i < TRK_MAX_PISTAS; i++) {
        Pista &p = T->pistas[i];
        if (!p.activa || pista_asignada[i]) continue;
        if (++p.perdidos > c.max_perdidos) nout = cerrar(T, p, out, nout);
    }

    // Detecciones sin pista: nacen pistas nuevas mientras haya espacio.
    for (int j = 0; j < nd; j++) {
        if (det_usada[j]) continue;
        for (int i = 0; i < TRK_MAX_PISTAS; i++) {
            Pista &p = T->pistas[i];
            if (p.activa) continue;
            p.activa = true; p.id = T->siguiente_id++; p.n = 1; p.perdidos = 0;
            p.m[0].t = t; p.m[0].x = det[j][0]; p.m[0].y = det[j][1];
            break;
        }
    }
    return nout;
}

int trk_cerrar_todo(Tracker *T, Resultado *out)
{
    int nout = 0;
    for (int i = 0; i < TRK_MAX_PISTAS; i++)
        if (T->pistas[i].activa) nout = cerrar(T, T->pistas[i], out, nout);
    return nout;
}
