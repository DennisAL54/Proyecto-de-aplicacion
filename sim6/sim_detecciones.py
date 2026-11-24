#!/usr/bin/env python3
"""Genera datos simulados con verdad conocida para probar el seguimiento, la homografía y la velocidad.

Simula una cámara fija con perspectiva y 10 vehículos con velocidades conocidas en dos carriles y dos
sentidos. En lugar de imágenes, produce lo que entregaría el detector de movimiento: el punto de contacto
con el suelo de cada vehículo en píxeles (320x240), con ruido, pérdidas y falsas detecciones.

Salidas:
  calibracion_sim.json   4 puntos imagen <-> mundo (marcados "a mano", redondeados a píxel)
  detecciones_sim.csv    cuadro,t_s,gx,gy            (lo que recibe el seguimiento)
  verdad_sim.csv         id,sentido,carril_x_m,v_kmh,t_medio_s
"""
import csv
import json
import math

import numpy as np

# ------------------ mundo simulado (valores controlados) ------------------
FPS = 7.0                       # cuadros por segundo de la cámara
DURACION_S = 90.0
TRAMO_Y = (6.0, 30.0)           # tramo medido: 24 m de largo
ANCHO = 7.0                     # ancho de la calzada, dos carriles
RUIDO_PX = 1.0                  # ruido del punto de contacto (desviación estándar, píxeles)
PERDIDA = 0.05                  # probabilidad de no detectar a un vehículo en un cuadro
FALSOS = 0.02                   # probabilidad de una falsa detección por cuadro
JITTER_S = 0.005                # variación de la marca de tiempo de cada cuadro
# id, sentido (+1 se aleja de la cámara, -1 se acerca), carril X (m), velocidad (km/h), t0 (s)
VEHICULOS = [
    (1, +1, 1.75, 40, 4.0), (2, -1, 5.25, 55, 9.0), (3, +1, 1.75, 60, 20.0), (4, +1, 1.75, 30, 30.0),
    (5, -1, 5.25, 45, 33.0), (6, -1, 5.25, 70, 48.0), (7, +1, 1.75, 50, 55.0), (8, -1, 5.25, 35, 62.0),
    (9, +1, 1.75, 65, 63.0), (10, +1, 1.75, 25, 75.0),
]
LARGO_AUTO = 4.2                # el punto de contacto es la parte trasera: Yc - 2.1 m
# ---------------------------------------------------------------------------

# Cámara simulada (pinhole): 320x240, f = 300 px, a 6 m de altura, inclinada 22 grados
FX = FY = 300.0
CX, CY = 160.0, 120.0
CAM = np.array([-1.5, -10.0, 6.0])
CAB = math.radians(22.0)
_Z = np.array([0.0, math.cos(CAB), -math.sin(CAB)])
_X = np.array([1.0, 0.0, 0.0])
R = np.vstack([_X, np.cross(_Z, _X), _Z])


def proyectar(p):
    pc = R @ (np.asarray(p, float) - CAM)
    return np.array([FX * pc[0] / pc[2] + CX, FY * pc[1] / pc[2] + CY])


def posicion(v, t):
    _, sentido, _, kmh, t0 = v
    if t < t0:
        return None
    y0 = -3.0 if sentido > 0 else 62.0
    y = y0 + sentido * (kmh / 3.6) * (t - t0)
    return y if -3.0 <= y <= 62.0 else None


rng = np.random.default_rng(42)

# calibración "a mano": 4 esquinas del tramo en píxeles, con error de redondeo
mundo = [(0.0, TRAMO_Y[0]), (ANCHO, TRAMO_Y[0]), (ANCHO, TRAMO_Y[1]), (0.0, TRAMO_Y[1])]
imagen = [np.round(proyectar((x, y, 0.0))).tolist() for x, y in mundo]
json.dump({"puntos_imagen": imagen, "puntos_mundo": [list(m) for m in mundo],
           "tramo": {"x": [0.0, ANCHO], "y": list(TRAMO_Y)}}, open("calibracion_sim.json", "w"), indent=1)

n_cuadros = int(DURACION_S * FPS)
with open("detecciones_sim.csv", "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["cuadro", "t_s", "gx", "gy"])
    for i in range(n_cuadros):
        t = i / FPS + rng.normal(0, JITTER_S)
        t_ideal = i / FPS
        for v in VEHICULOS:
            y = posicion(v, t_ideal)
            if y is None or rng.random() < PERDIDA:
                continue
            u, vv = proyectar((v[2], y - LARGO_AUTO / 2, 0.0))
            if 0 <= u < 320 and 0 <= vv < 240:
                w.writerow([i, f"{t:.4f}", f"{u + rng.normal(0, RUIDO_PX):.2f}", f"{vv + rng.normal(0, RUIDO_PX):.2f}"])
        if rng.random() < FALSOS:                                     # falsa detección en cualquier punto de la calzada
            u, vv = proyectar((rng.uniform(0, ANCHO), rng.uniform(TRAMO_Y[0], TRAMO_Y[1]), 0.0))
            w.writerow([i, f"{t:.4f}", f"{u:.2f}", f"{vv:.2f}"])

ymid = 0.5 * (TRAMO_Y[0] + TRAMO_Y[1]) + LARGO_AUTO / 2             # el centro del auto cuando el contacto está a mitad del tramo
with open("verdad_sim.csv", "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["id", "sentido", "carril_x_m", "v_kmh", "t_medio_s"])
    for (cid, s, x, kmh, t0) in VEHICULOS:
        y0 = -3.0 if s > 0 else 62.0
        w.writerow([cid, s, x, kmh, f"{t0 + (ymid - y0) / (s * kmh / 3.6):.3f}"])
print(f"Listo: {n_cuadros} cuadros a {FPS} FPS, tramo de {TRAMO_Y[1]-TRAMO_Y[0]:.0f} m, {len(VEHICULOS)} vehículos")
