#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "registro.h"

static int fallos = 0;
static void verificar(const char *n, bool ok, const char *d) { printf("   %-56s %s  %s\n", n, ok ? "OK   " : "FALLA", d); if (!ok) fallos++; }

static int contar_lineas(const char *ruta) { FILE *f = fopen(ruta, "r"); if (!f) return -1; int n = 0, c; while ((c = fgetc(f)) != EOF) if (c == '\n') n++; fclose(f); return n; }

int main()
{
    char buf[200], d[200];

    printf("1) Conversión de fechas\n");
    long long e;
    bool ok = texto_a_epoch("2020-01-01 00:00:00", &e);
    verificar("2020-01-01 00:00:00 -> 1577836800", ok && e == 1577836800LL, "");
    ok = texto_a_epoch("2026-11-21 19:48:00", &e);
    snprintf(d, sizeof d, "epoch = %lld", e);
    verificar("2026-11-21 19:48:00 -> 1795290480", ok && e == 1795290480LL, d);
    epoch_a_texto(e, buf);
    verificar("ida y vuelta devuelve el mismo texto", strcmp(buf, "2026-11-21 19:48:00") == 0, buf);
    texto_a_epoch("2028-02-29 23:59:59", &e); epoch_a_texto(e + 1, buf);
    verificar("año bisiesto: 29/02/2028 + 1 s = 01/03/2028", strcmp(buf, "2028-03-01 00:00:00") == 0, buf);
    texto_a_epoch("2026-12-31 23:59:59", &e); epoch_a_texto(e + 1, buf);
    verificar("cambio de año: 31/12/2026 + 1 s = 01/01/2027", strcmp(buf, "2027-01-01 00:00:00") == 0, buf);
    verificar("fecha inexistente (30/02) se rechaza", !texto_a_epoch("2026-02-30 10:00:00", &e), "");
    verificar("texto basura se rechaza", !texto_a_epoch("hola", &e), "");
    epoch_a_nombre(1795290480LL, buf);
    verificar("nombre de archivo AAAAMMDD_HHMMSS", strcmp(buf, "20261121_194800") == 0, buf);

    printf("2) hora.txt y reloj\n");
    char dir[] = "/tmp/regXXXXXX";
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(buf, sizeof buf, "%s/hora.txt", dir);
    FILE *f = fopen(buf, "w"); fputs("2026-11-21 19:48:00\n", f); fclose(f);
    Reloj R;
    verificar("lee hora.txt", reloj_leer_archivo(buf, 12.3, &R), "");
    epoch_a_texto(reloj_epoch(R, 12.3 + 3600.4), d);
    verificar("una hora después del arranque -> 20:48:00", strcmp(d, "2026-11-21 20:48:00") == 0, d);
    snprintf(buf, sizeof buf, "%s/no_existe.txt", dir);
    verificar("sin hora.txt devuelve false", !reloj_leer_archivo(buf, 0, &R), "");

    printf("3) Registro CSV y cambio de archivo a las 24 h\n");
    Registro G; reg_iniciar(&G, dir);
    Resultado r = {};
    long long t0 = 1795290480LL;
    r.id = 1; r.kmh = 40.16; r.x_medio = 1.754;
    verificar("primer registro", reg_agregar(&G, t0, r) == REG_OK, "");
    r.id = 2; r.kmh = 55.04; r.x_medio = 5.21;
    verificar("segundo registro (misma hora)", reg_agregar(&G, t0 + 3600, r) == REG_OK, "");
    char primero[160]; snprintf(primero, sizeof primero, "%s", G.archivo);
    snprintf(d, sizeof d, "%d líneas", contar_lineas(primero));
    verificar("el archivo tiene encabezado + 2 filas (3 líneas)", contar_lineas(primero) == 3, d);
    r.id = 3; r.kmh = 30.0;
    reg_agregar(&G, t0 + 86399, r);
    verificar("a las 23:59:59 sigue el mismo archivo", strcmp(primero, G.archivo) == 0, "");
    r.id = 4; r.kmh = 45.0;
    reg_agregar(&G, t0 + 86400, r);
    verificar("a las 24 h se crea un archivo nuevo", strcmp(primero, G.archivo) != 0, strrchr(G.archivo, '/') + 1);
    snprintf(d, sizeof d, "%d líneas", contar_lineas(G.archivo));
    verificar("el archivo nuevo tiene encabezado + 1 fila", contar_lineas(G.archivo) == 2, d);
    snprintf(d, sizeof d, "%d líneas", contar_lineas(primero));
    verificar("el primero quedó con encabezado + 3 filas", contar_lineas(primero) == 4, d);

    printf("4) Errores\n");
    Registro G2; reg_iniciar(&G2, "/ruta/que/no/existe");
    verificar("carpeta inexistente devuelve error de apertura", reg_agregar(&G2, t0, r) == REG_ERR_ABRIR, "");

    printf("\nContenido del primer archivo:\n"); fflush(stdout);
    snprintf(buf, sizeof buf, "cat %s", primero); if (system(buf)) {}
    snprintf(buf, sizeof buf, "rm -rf %s", dir); if (system(buf)) {}
    printf("\n%s\n", fallos == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY PRUEBAS QUE FALLARON");
    return fallos ? 1 : 0;
}
