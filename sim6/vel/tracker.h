#pragma once
#include "velocidad.h"

#define TRK_MAX_PISTAS 4          // REQ-REND-004: hasta 4 vehículos a la vez

struct TrkConfig {
    double compuerta_m;           // distancia máxima entre la posición predicha y la detección (m)
    double lateral_m;             // desplazamiento lateral máximo permitido entre cuadros (m)
    int    max_perdidos;          // cuadros seguidos sin detección antes de cerrar la pista
    int    min_muestras;          // muestras mínimas para dar una velocidad
    double min_recorrido_m;       // recorrido mínimo para dar una velocidad (m)
};

// Velocidad de una pista ya cerrada.
struct Resultado {
    int    id;
    double kmh;
    int    muestras;
    double t_inicio, t_fin;
    double x_medio;               // posición lateral media (m), identifica el carril
};

struct Pista {
    bool    activa;
    int     id;
    int     n;
    int     perdidos;
    Muestra m[VEL_MAX_MUESTRAS];
};

struct Tracker {
    TrkConfig cfg;
    Pista     pistas[TRK_MAX_PISTAS];
    int       siguiente_id;
};

TrkConfig trk_config_defecto();
void trk_iniciar(Tracker *T, const TrkConfig &cfg);

// Procesa un cuadro: t = instante (s); det[i] = {X, Y} en metros del punto de contacto con el suelo.
// Escribe en out las pistas que se cerraron en este cuadro y devuelve cuántas (máx. TRK_MAX_PISTAS).
int trk_actualizar(Tracker *T, double t, const double (*det)[2], int nd, Resultado *out);

// Cierra todas las pistas abiertas (al terminar la grabación). Devuelve cuántos resultados hay en out.
int trk_cerrar_todo(Tracker *T, Resultado *out);
