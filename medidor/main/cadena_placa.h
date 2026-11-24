#pragma once

// Ensayo de la cadena de medición en la placa, con datos simulados guardados en la microSD:
//   /sdcard/hora.txt              primera línea: AAAA-MM-DD HH:MM:SS
//   /sdcard/detecciones_sim.csv   cuadro,t_s,gx,gy
// Escribe los CSV en /sdcard/salida/ y muestra por el monitor el resumen y los tiempos.
// La SD ya debe estar montada (sd_mount()).
void cadena_ensayo_sd(void);
