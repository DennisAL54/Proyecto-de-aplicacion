#pragma once
#include "esp_err.h"

esp_err_t   sd_mount(void);
void        sd_unmount(void);
bool        sd_nuevo_archivo(void);
const char *sd_nombre_actual(void);
bool        sd_registro_vehiculo(int id_tramo, float longitud, int id_veh, float vel_kmh);
float       sd_porcentaje_usado(void);
bool        sd_borrar_mas_antiguo(const char *excluir);
void        sd_listar(void);
