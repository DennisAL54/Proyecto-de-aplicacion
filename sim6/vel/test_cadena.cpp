// Prueba del módulo cadena: lee detecciones_sim.csv en streaming (como llegarían del detector) y la procesa cuadro a cuadro.
// Uso: ./test_cadena carpeta_salida hora.txt [detecciones_sim.csv]   -> escribe también resultados_sim.csv
#include <stdio.h>
#include "cadena.h"

static const Homografia H = {{3.111111, -0.729167, -497.0, 0.0, -6.125, 1014.0, 0.0, 0.479167, 1.0}};

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s carpeta_salida hora.txt [detecciones.csv]\n", argv[0]); return 1; }
    FILE *f = fopen(argc > 3 ? argv[3] : "detecciones_sim.csv", "r");
    if (!f) { perror("detecciones"); return 1; }
    Cadena C;
    if (!cad_iniciar(&C, H, argv[1], argv[2], 0.0)) { fprintf(stderr, "hora.txt inválida\n"); return 1; }
    FILE *g = fopen("resultados_sim.csv", "w");
    fprintf(g, "id,kmh,t_inicio,t_fin,x_medio_m\n");

    char linea[256];
    if (!fgets(linea, sizeof linea, f)) return 1;
    double det[CAD_MAX_DET][2]; int nd = 0, cuadro_act = -1; double t_act = 0, dt = 1.0 / 7.0;
    Resultado r[TRK_MAX_PISTAS];
    auto cerrar_cuadro = [&]() {
        int n = cad_cuadro(&C, t_act, det, nd, r);
        for (int i = 0; i < n; i++) fprintf(g, "%d,%.3f,%.3f,%.3f,%.3f\n", r[i].id, r[i].kmh, r[i].t_inicio, r[i].t_fin, r[i].x_medio);
        nd = 0;
    };
    while (fgets(linea, sizeof linea, f)) {
        int c; double t, u, v;
        if (sscanf(linea, "%d,%lf,%lf,%lf", &c, &t, &u, &v) != 4) continue;
        if (cuadro_act < 0) cuadro_act = c;
        while (cuadro_act < c) {                 // cuadros que cambian: se procesa el anterior y los vacíos intermedios
            cerrar_cuadro();
            cuadro_act++;
            t_act += dt;
        }
        t_act = t;
        if (nd < CAD_MAX_DET) { det[nd][0] = u; det[nd][1] = v; nd++; }
    }
    if (cuadro_act >= 0) cerrar_cuadro();
    int n = cad_cerrar(&C, r);
    for (int i = 0; i < n; i++) fprintf(g, "%d,%.3f,%.3f,%.3f,%.3f\n", r[i].id, r[i].kmh, r[i].t_inicio, r[i].t_fin, r[i].x_medio);
    fclose(g); fclose(f);
    printf("%d vehículos registrados en %s (errores de escritura: %d)\n", C.registrados, argv[1], C.errores);
    return C.errores ? 2 : 0;
}
