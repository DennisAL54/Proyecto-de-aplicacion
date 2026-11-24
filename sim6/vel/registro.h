#pragma once
#include "tracker.h"

// ---------- Reloj: hora real = hora de hora.txt + tiempo transcurrido desde el arranque ----------
struct Reloj {
    long long base_epoch;     // segundos desde 1970-01-01 00:00:00 de la hora escrita en hora.txt (hora local, sin zona)
    double    t_base;         // instante del arranque (s) en que se leyó la hora
};

// Convierte "AAAA-MM-DD HH:MM:SS" a segundos desde 1970. Devuelve false si el texto no es una fecha válida.
bool texto_a_epoch(const char *txt, long long *epoch);
// Escribe "AAAA-MM-DD HH:MM:SS" en buf (mínimo 20 bytes).
void epoch_a_texto(long long epoch, char *buf);
// Escribe "AAAAMMDD_HHMMSS" en buf (mínimo 16 bytes).
void epoch_a_nombre(long long epoch, char *buf);

// Lee la primera línea de hora.txt (ruta completa) y fija el reloj; t_ahora es el instante actual del arranque (s).
bool reloj_leer_archivo(const char *ruta, double t_ahora, Reloj *R);
// Hora real (epoch) para un instante t del arranque.
long long reloj_epoch(const Reloj &R, double t);

// ---------- Registro CSV: un archivo nuevo cada 24 h de operación ----------
#define REG_PERIODO_S 86400LL

enum RegEstado { REG_OK = 0, REG_ERR_ABRIR = 1, REG_ERR_ESCRIBIR = 2 };

struct Registro {
    char      dir[96];        // carpeta de destino ("/sdcard" en la placa)
    char      archivo[160];   // archivo actual (vacío si todavía no hay)
    long long inicio_epoch;   // hora en que se creó el archivo actual
};

void reg_iniciar(Registro *G, const char *dir);
// Agrega una fila para el vehículo r, ocurrido en la hora epoch. Cambia de archivo si pasaron 24 h.
// Abre, escribe y cierra en cada llamada, para no perder datos si se corta la energía.
RegEstado reg_agregar(Registro *G, long long epoch, const Resultado &r);
