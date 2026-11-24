#include "cadena_placa.h"
#include "cadena.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "cadena";

// Homografía de la calibración simulada (la que dio test_hom en la computadora).
static const Homografia H_SIM = {{3.111111, -0.729167, -497.0, 0.0, -6.125, 1014.0, 0.0, 0.479167, 1.0}};

// Estructuras grandes fuera de la pila: la tarea principal tiene poca pila.
static Cadena C;

struct Medidor {
    int64_t suma_us = 0, max_us = 0;
    int n = 0, lentos = 0;
};

static void procesar(double t, const double (*det)[2], int nd, Medidor *M, FILE *g)
{
    Resultado r[TRK_MAX_PISTAS];
    int64_t t0 = esp_timer_get_time();
    int n = cad_cuadro(&C, t, det, nd, r);
    int64_t dt = esp_timer_get_time() - t0;
    M->suma_us += dt; M->n++;
    if (dt > M->max_us) M->max_us = dt;
    if (dt > 140000) M->lentos++;                 // el presupuesto a 7 FPS es de unos 140 ms por cuadro
    for (int i = 0; i < n; i++)
        fprintf(g, "%d,%.3f,%.3f,%.3f,%.3f\n", r[i].id, r[i].kmh, r[i].t_inicio, r[i].t_fin, r[i].x_medio);
}

void cadena_ensayo_sd(void)
{
    ESP_LOGI(TAG, "inicio del ensayo. Heap libre: %u, PSRAM libre: %u",
             (unsigned)esp_get_free_heap_size(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    mkdir("/sdcard/salida", 0777);
    if (!cad_iniciar(&C, H_SIM, "/sdcard/salida", "/sdcard/hora.txt", 0.0)) {
        ESP_LOGE(TAG, "no se pudo leer /sdcard/hora.txt (formato AAAA-MM-DD HH:MM:SS): error de arranque");
        return;
    }
    FILE *f = fopen("/sdcard/detecciones_sim.csv", "r");
    if (!f) { ESP_LOGE(TAG, "falta /sdcard/detecciones_sim.csv"); return; }
    FILE *g = fopen("/sdcard/resultados_sim.csv", "w");
    if (!g) { ESP_LOGE(TAG, "no se pudo crear /sdcard/resultados_sim.csv"); fclose(f); return; }
    fprintf(g, "id,kmh,t_inicio,t_fin,x_medio_m\n");

    Medidor M;
    char linea[128];
    if (!fgets(linea, sizeof linea, f)) { fclose(f); fclose(g); return; }        // encabezado
    double det[CAD_MAX_DET][2]; int nd = 0, cuadro_act = -1; double t_act = 0;
    const double dt_cuadro = 1.0 / 7.0;
    int64_t t_ini = esp_timer_get_time();
    while (fgets(linea, sizeof linea, f)) {
        int c; double t, u, v;
        if (sscanf(linea, "%d,%lf,%lf,%lf", &c, &t, &u, &v) != 4) continue;
        if (cuadro_act < 0) cuadro_act = c;
        while (cuadro_act < c) {                  // se procesa el cuadro anterior y los vacíos intermedios
            procesar(t_act, det, nd, &M, g);
            nd = 0; cuadro_act++; t_act += dt_cuadro;
        }
        t_act = t;
        if (nd < CAD_MAX_DET) { det[nd][0] = u; det[nd][1] = v; nd++; }
    }
    if (cuadro_act >= 0) procesar(t_act, det, nd, &M, g);
    Resultado r[TRK_MAX_PISTAS];
    int n = cad_cerrar(&C, r);
    for (int i = 0; i < n; i++)
        fprintf(g, "%d,%.3f,%.3f,%.3f,%.3f\n", r[i].id, r[i].kmh, r[i].t_inicio, r[i].t_fin, r[i].x_medio);
    fclose(g); fclose(f);
    int64_t total_ms = (esp_timer_get_time() - t_ini) / 1000;

    ESP_LOGI(TAG, "vehículos registrados: %d, errores de escritura: %d", C.registrados, C.errores);
    ESP_LOGI(TAG, "cuadros procesados: %d. Tiempo por cuadro: medio %lld us, máximo %lld us, cuadros sobre 140 ms: %d",
             M.n, M.n ? (long long)(M.suma_us / M.n) : 0LL, (long long)M.max_us, M.lentos);
    ESP_LOGI(TAG, "tiempo total del ensayo (incluye leer la SD): %lld ms", (long long)total_ms);
    ESP_LOGI(TAG, "pila libre mínima de la tarea: %u bytes. Heap libre: %u",
             (unsigned)uxTaskGetStackHighWaterMark(NULL) * (unsigned)sizeof(StackType_t), (unsigned)esp_get_free_heap_size());
    ESP_LOGI(TAG, "fin. Revise /sdcard/salida/ y /sdcard/resultados_sim.csv en la computadora");
}
