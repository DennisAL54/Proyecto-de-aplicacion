// Paso 10.6: cadena completa sobre datos simulados.
//   detecciones_sim.csv (píxeles) -> homografía -> zona de medición -> Tracker -> reloj (hora.txt) -> CSV con fecha y hora
// Uso: ./cadena_completa carpeta_salida hora.txt [detecciones_sim.csv]
// Además escribe resultados_sim.csv (el formato de comparar_cadena.py) en la carpeta actual.
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include "homografia.h"
#include "tracker.h"
#include "registro.h"

// Homografía de la calibración simulada (la que dio test_hom).
static const Homografia H = {{3.111111, -0.729167, -497.0, 0.0, -6.125, 1014.0, 0.0, 0.479167, 1.0}};
// Zona de medición: solo se usan detecciones con Y dentro del tramo calibrado (m).
static const double ZONA_Y_MIN = -2.0, ZONA_Y_MAX = 26.0;

struct Det { double t, u, v; };

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s carpeta_salida hora.txt [detecciones_sim.csv]\n", argv[0]); return 1; }
    const char *carpeta = argv[1], *ruta_hora = argv[2];
    const char *entrada = argc > 3 ? argv[3] : "detecciones_sim.csv";

    FILE *f = fopen(entrada, "r");
    if (!f) { perror(entrada); return 1; }
    std::map<int, std::vector<Det>> cuadros;
    char linea[256];
    if (!fgets(linea, sizeof linea, f)) return 1;
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

    // El instante 0 de la simulación es el momento en que se leyó hora.txt (el arranque del dispositivo).
    Reloj R;
    if (!reloj_leer_archivo(ruta_hora, 0.0, &R)) {
        fprintf(stderr, "no se pudo leer %s (formato: AAAA-MM-DD HH:MM:SS). En la placa esto sería un error de arranque.\n", ruta_hora);
        return 1;
    }

    Registro G; reg_iniciar(&G, carpeta);
    FILE *g = fopen("resultados_sim.csv", "w");
    fprintf(g, "id,kmh,t_inicio,t_fin,x_medio_m\n");
    Tracker T; trk_iniciar(&T, trk_config_defecto());
    Resultado r[TRK_MAX_PISTAS];
    int total = 0, errores = 0;
    double dt_cuadro = 1.0 / 7.0, t_ult = 0;

    auto guardar = [&](int n) {
        for (int i = 0; i < n; i++) {
            fprintf(g, "%d,%.3f,%.3f,%.3f,%.3f\n", r[i].id, r[i].kmh, r[i].t_inicio, r[i].t_fin, r[i].x_medio);
            RegEstado e = reg_agregar(&G, reloj_epoch(R, r[i].t_fin), r[i]);   // la hora del evento es cuando el vehículo sale
            if (e != REG_OK) errores++;
            total++;
        }
    };

    for (int c = cmin; c <= cmax; c++) {
        double det[16][2]; int nd = 0; double t = t_ult + dt_cuadro;
        auto it = cuadros.find(c);
        if (it != cuadros.end()) {
            t = it->second[0].t;
            for (const Det &d : it->second) {
                if (nd >= 16) break;
                hom_aplicar(H, d.u, d.v, &det[nd][0], &det[nd][1]);
                if (det[nd][1] >= ZONA_Y_MIN && det[nd][1] <= ZONA_Y_MAX) nd++;
            }
        }
        t_ult = t;
        guardar(trk_actualizar(&T, t, det, nd, r));
    }
    guardar(trk_cerrar_todo(&T, r));
    fclose(g);
    printf("%d vehículos registrados en %s (errores de escritura: %d)\n", total, carpeta, errores);
    return errores ? 2 : 0;
}
