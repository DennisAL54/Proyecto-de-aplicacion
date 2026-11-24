#pragma once
// Ensayo en vivo: cámara -> detector de movimiento -> cadena de medición -> CSV en la SD.
// Archivos en la raíz de la SD:
//   hora.txt          AAAA-MM-DD HH:MM:SS (hora de arranque)
//   calibracion.txt   4 líneas "u v X Y" (píxel de la imagen 320x240 y su posición en metros) y, opcional,
//                     una línea "zona ymin ymax" con la zona de medición en metros
// Escribe en /sdcard/salida/ los CSV de velocidades y en /sdcard/det_vivo.csv las detecciones crudas
// (cuadro,t_s,gx,gy), que sirven para repetir la prueba en la computadora.
// La SD debe estar montada (sd_mount()). Devuelve cuando pasan duracion_s segundos.
void vivo_ejecutar(int duracion_s);
