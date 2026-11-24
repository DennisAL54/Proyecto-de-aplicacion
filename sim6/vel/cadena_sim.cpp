// Paso 10.4b: lee detecciones_sim.csv (cuadro,t_s,gx,gy con gx,gy en píxeles), pasa a metros con la homografía
// y alimenta al Tracker. Escribe resultados_sim.csv: id,kmh,t_inicio,t_fin,x_medio_m
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>
#include "homografia.h"
#include "tracker.h"

// H de la calibración simulada (la que dio test_hom). Si cambia calibracion_sim.json, actualícela aquí.
static const Homografia H = {{3.111111, -0.729167, -497.0, 0.0, -6.125, 1014.0, 0.0, 0.479167, 1.0}};

struct Det { double t, u, v; };

int main(int argc, char **argv)
{
    const char *entrada = argc > 1 ? argv[1] : "detecciones_sim.csv";
    const char *salida  = argc > 2 ? argv[2] : "resultados_sim.csv";
    // Parámetros opcionales para experimentar: compuerta_m lateral_m max_perdidos min_muestras min_recorrido_m
    TrkConfig cfg = trk_config_defecto();
    if (argc > 3) cfg.compuerta_m = atof(argv[3]);
    if (argc > 4) cfg.lateral_m = atof(argv[4]);
    if (argc > 5) cfg.max_perdidos = atoi(argv[5]);
    if (argc > 6) cfg.min_muestras = atoi(argv[6]);
    if (argc > 7) cfg.min_recorrido_m = atof(argv[7]);
    // Zona de medición: solo se usan detecciones con Y entre ymin y ymax (m). Por defecto, sin límite.
    double ymin = argc > 8 ? atof(argv[8]) : -1e9, ymax = argc > 9 ? atof(argv[9]) : 1e9;
    FILE *f = fopen(entrada, "r");
    if (!f) { perror(entrada); return 1; }
    std::map<int, std::vector<Det>> cuadros;
    char linea[256];
    if (!fgets(linea, sizeof linea, f)) return 1;        // encabezado
    int cmin = 1 << 30, cmax = -1;
    while (fgets(linea, sizeof linea, f)) {
        int c; double t, u, v;
        if (sscanf(linea, "%d,%lf,%lf,%lf", &c, &t, &u, &v) != 4) continue;
        cuadros[c].push_back({t, u, v});
        if (c < cmin) cmin = c;
        if (c > cmax) cmax = c;
    }
    fclose(f);
    if (cmax < 0) { fprintf(stderr, "sin datos\n"); return 1; }

    FILE *g = fopen(salida, "w");
    fprintf(g, "id,kmh,t_inicio,t_fin,x_medio_m\n");
    Tracker T; trk_iniciar(&T, cfg);
    Resultado r[TRK_MAX_PISTAS];
    int total = 0;
    double dt_cuadro = 1.0 / 7.0, t_ult = 0;
    for (int c = cmin; c <= cmax; c++) {                 // se llama en TODOS los cuadros, también en los vacíos
        double det[16][2]; int nd = 0; double t = t_ult + dt_cuadro;
        auto it = cuadros.find(c);
        if (it != cuadros.end()) {
            t = it->second[0].t;
            for (const Det &d : it->second) { if (nd < 16) { hom_aplicar(H, d.u, d.v, &det[nd][0], &det[nd][1]); if (det[nd][1] >= ymin && det[nd][1] <= ymax) nd++; } }
        }
        t_ult = t;
        int n = trk_actualizar(&T, t, det, nd, r);
        for (int i = 0; i < n; i++) { fprintf(g, "%d,%.3f,%.3f,%.3f,%.3f\n", r[i].id, r[i].kmh, r[i].t_inicio, r[i].t_fin, r[i].x_medio); total++; }
    }
    int n = trk_cerrar_todo(&T, r);
    for (int i = 0; i < n; i++) { fprintf(g, "%d,%.3f,%.3f,%.3f,%.3f\n", r[i].id, r[i].kmh, r[i].t_inicio, r[i].t_fin, r[i].x_medio); total++; }
    fclose(g);
    printf("%d cuadros leídos (%d a %d), %d vehículos registrados -> %s\n", (int)cuadros.size(), cmin, cmax, total + 0, salida);
    return 0;
}
