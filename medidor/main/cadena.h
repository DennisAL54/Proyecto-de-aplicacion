#pragma once
// Cadena de medición: detecciones en píxeles -> metros -> seguimiento -> velocidad -> CSV con fecha y hora.
// No depende de ESP-IDF ni de archivos de entrada: recibe un cuadro a la vez, igual que en la placa.
#include "homografia.h"
#include "tracker.h"
#include "registro.h"

// Zona de medición: solo se usan detecciones con Y dentro del tramo calibrado (m).
#define CAD_ZONA_Y_MIN (-2.0)
#define CAD_ZONA_Y_MAX (26.0)
#define CAD_MAX_DET 16

struct Cadena {
    Homografia H;
    Tracker    T;
    Registro   G;
    Reloj      R;
    double     zona_ymin, zona_ymax;   // zona de medición (m)
    double     t_ult;
    int        registrados;       // vehículos escritos con éxito
    int        errores;           // fallos de escritura
};

// carpeta: donde se escriben los CSV ("/sdcard" en la placa). ruta_hora: archivo hora.txt.
// t_arranque: instante actual (s) del reloj del sistema, que corresponde a la hora de hora.txt.
// Devuelve false si no se pudo leer una hora válida (en la placa: error de arranque).
bool cad_iniciar(Cadena *C, const Homografia &H, const char *carpeta, const char *ruta_hora, double t_arranque);

// Cambia la zona de medición (por defecto CAD_ZONA_Y_MIN a CAD_ZONA_Y_MAX). Llamar después de cad_iniciar.
void cad_zona(Cadena *C, double ymin, double ymax);

// Procesa un cuadro. t = instante (s); det_px[i] = {u, v} píxel del punto de contacto con el suelo.
// Hay que llamarla en TODOS los cuadros, incluso sin detecciones (nd = 0). Devuelve cuántos vehículos se cerraron.
// Si res no es NULL, copia ahí los resultados (capacidad TRK_MAX_PISTAS).
int cad_cuadro(Cadena *C, double t, const double (*det_px)[2], int nd, Resultado *res);

// Cierra las pistas abiertas (al apagar o terminar). Devuelve cuántos vehículos se cerraron.
int cad_cerrar(Cadena *C, Resultado *res);
