#!/usr/bin/env python3
"""Compara las velocidades calculadas con la verdad simulada.

Uso:  python3 comparar_sim.py resultados.csv [verdad_sim.csv]
El archivo de resultados (lo que imprime su programa de C++) debe tener la cabecera:
    pista,t_medio_s,x_medio_m,velocidad_kmh,muestras
"""
import csv
import sys

import numpy as np

res = list(csv.DictReader(open(sys.argv[1])))
gt = list(csv.DictReader(open(sys.argv[2] if len(sys.argv) > 2 else "verdad_sim.csv")))
usadas, errores = set(), []
print(f"{'id':>3} {'real':>6} {'calculada':>10} {'error':>7}")
for g in gt:
    mejor, dmin = None, 1e9
    for i, r in enumerate(res):
        if i in usadas:
            continue
        dt = abs(float(r["t_medio_s"]) - float(g["t_medio_s"]))
        dx = abs(float(r["x_medio_m"]) - float(g["carril_x_m"]))
        if dt <= 3.0 and dx <= 1.8 and dt < dmin:
            mejor, dmin = i, dt
    if mejor is None:
        print(f"{g['id']:>3} {float(g['v_kmh']):>6.0f} {'NO DETECTADO':>10}")
    else:
        usadas.add(mejor)
        v = float(res[mejor]["velocidad_kmh"])
        errores.append(v - float(g["v_kmh"]))
        print(f"{g['id']:>3} {float(g['v_kmh']):>6.0f} {v:>10.1f} {errores[-1]:>+7.1f}")
falsos = len(res) - len(usadas)
print("-" * 36)
print(f"Detectados: {len(errores)} de {len(gt)} | pistas falsas: {falsos}")
if errores:
    e = np.abs(errores)
    print(f"Error medio: {e.mean():.2f} km/h | máximo: {e.max():.2f} km/h | dentro de ±5 km/h: {int((e <= 5).sum())} de {len(e)}")
