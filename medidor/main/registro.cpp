#include "registro.h"
#include <stdio.h>
#include <string.h>

// ---- Calendario (algoritmos de días desde 1970, sin depender de la zona horaria del sistema) ----
static long long dias_desde_civil(long long y, int m, int d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_desde_dias(long long z, int *y, int *m, int *d)
{
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}

static bool bisiesto(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

bool texto_a_epoch(const char *txt, long long *epoch)
{
    int Y, M, D, h, mi, s;
    char extra;
    if (sscanf(txt, "%d-%d-%d %d:%d:%d%c", &Y, &M, &D, &h, &mi, &s, &extra) < 6) return false;
    static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (Y < 2020 || Y > 2099 || M < 1 || M > 12 || h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) return false;
    int maxd = dm[M - 1] + (M == 2 && bisiesto(Y) ? 1 : 0);
    if (D < 1 || D > maxd) return false;
    *epoch = dias_desde_civil(Y, M, D) * 86400LL + h * 3600 + mi * 60 + s;
    return true;
}

void epoch_a_texto(long long e, char *buf)
{
    long long dias = e >= 0 ? e / 86400 : -((-e + 86399) / 86400);
    long long seg = e - dias * 86400;
    int Y, M, D; civil_desde_dias(dias, &Y, &M, &D);
    snprintf(buf, 20, "%04d-%02d-%02d %02u:%02u:%02u", Y, M, D, (unsigned)(seg / 3600) % 24, (unsigned)(seg % 3600 / 60) % 60, (unsigned)(seg % 60) % 60);
}

void epoch_a_nombre(long long e, char *buf)
{
    long long dias = e >= 0 ? e / 86400 : -((-e + 86399) / 86400);
    long long seg = e - dias * 86400;
    int Y, M, D; civil_desde_dias(dias, &Y, &M, &D);
    snprintf(buf, 16, "%04d%02d%02d_%02u%02u%02u", Y, M, D, (unsigned)(seg / 3600) % 24, (unsigned)(seg % 3600 / 60) % 60, (unsigned)(seg % 60) % 60);
}

// ---- Reloj ----
bool reloj_leer_archivo(const char *ruta, double t_ahora, Reloj *R)
{
    FILE *f = fopen(ruta, "r");
    if (!f) return false;
    char linea[64] = {};
    bool ok = fgets(linea, sizeof linea, f) != NULL;
    fclose(f);
    if (!ok) return false;
    long long e;
    if (!texto_a_epoch(linea, &e)) return false;
    R->base_epoch = e; R->t_base = t_ahora;
    return true;
}

long long reloj_epoch(const Reloj &R, double t)
{
    double dt = t - R.t_base;
    return R.base_epoch + (long long)(dt + (dt >= 0 ? 0.5 : -0.5));
}

// ---- Registro ----
void reg_iniciar(Registro *G, const char *dir)
{
    snprintf(G->dir, sizeof G->dir, "%s", dir);
    G->archivo[0] = 0; G->inicio_epoch = 0;
}

RegEstado reg_agregar(Registro *G, long long epoch, const Resultado &r)
{
    bool nuevo = G->archivo[0] == 0 || epoch - G->inicio_epoch >= REG_PERIODO_S;
    if (nuevo) {
        char nom[16];
        epoch_a_nombre(epoch, nom);
        snprintf(G->archivo, sizeof G->archivo, "%s/datos_%s.csv", G->dir, nom);
        G->inicio_epoch = epoch;
    }
    FILE *f = fopen(G->archivo, "a");
    if (!f) { G->archivo[0] = 0; return REG_ERR_ABRIR; }       // se reintentará creando el archivo en la próxima llamada
    bool ok = true;
    if (nuevo) ok = fprintf(f, "fecha_hora,id,velocidad_kmh,carril_x_m\n") > 0;
    char fh[20];
    epoch_a_texto(epoch, fh);
    if (ok) ok = fprintf(f, "%s,%d,%.1f,%.2f\n", fh, r.id, r.kmh, r.x_medio) > 0;
    if (fclose(f) != 0) ok = false;
    return ok ? REG_OK : REG_ERR_ESCRIBIR;
}
