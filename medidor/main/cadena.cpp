#include "cadena.h"

bool cad_iniciar(Cadena *C, const Homografia &H, const char *carpeta, const char *ruta_hora, double t_arranque)
{
    C->H = H;
    C->zona_ymin = CAD_ZONA_Y_MIN; C->zona_ymax = CAD_ZONA_Y_MAX;
    C->carril_xmin = -1e9; C->carril_xmax = 1e9;
    C->t_ult = t_arranque; C->registrados = 0; C->errores = 0;
    trk_iniciar(&C->T, trk_config_defecto());
    reg_iniciar(&C->G, carpeta);
    return reloj_leer_archivo(ruta_hora, t_arranque, &C->R);
}

void cad_zona(Cadena *C, double ymin, double ymax) { C->zona_ymin = ymin; C->zona_ymax = ymax; }

void cad_carril(Cadena *C, double xmin, double xmax) { C->carril_xmin = xmin; C->carril_xmax = xmax; }

static int escribir(Cadena *C, const Resultado *r, int n)
{
    for (int i = 0; i < n; i++) {
        // La hora del evento es cuando el vehículo sale de la zona de medición (se cierra su pista).
        if (reg_agregar(&C->G, reloj_epoch(C->R, r[i].t_fin), r[i]) == REG_OK) C->registrados++;
        else C->errores++;
    }
    return n;
}

int cad_cuadro(Cadena *C, double t, const double (*det_px)[2], int nd, Resultado *res)
{
    double det[CAD_MAX_DET][2];
    int n = 0;
    for (int i = 0; i < nd && n < CAD_MAX_DET; i++) {
        hom_aplicar(C->H, det_px[i][0], det_px[i][1], &det[n][0], &det[n][1]);
        if (det[n][1] >= C->zona_ymin && det[n][1] <= C->zona_ymax &&
            det[n][0] >= C->carril_xmin && det[n][0] <= C->carril_xmax) n++;
    }
    C->t_ult = t;
    Resultado local[TRK_MAX_PISTAS];
    Resultado *r = res ? res : local;
    int k = trk_actualizar(&C->T, t, det, n, r);
    return escribir(C, r, k);
}

int cad_cerrar(Cadena *C, Resultado *res)
{
    Resultado local[TRK_MAX_PISTAS];
    Resultado *r = res ? res : local;
    int k = trk_cerrar_todo(&C->T, r);
    return escribir(C, r, k);
}
