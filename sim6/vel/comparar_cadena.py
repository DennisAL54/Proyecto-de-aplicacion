#!/usr/bin/env python3
"""Compara resultados_sim.csv (lo que midió la cadena) con verdad_sim.csv (lo que se simuló)."""
import csv, sys, statistics as st
res_f = sys.argv[1] if len(sys.argv) > 1 else "resultados_sim.csv"
ver_f = sys.argv[2] if len(sys.argv) > 2 else "verdad_sim.csv"
res = list(csv.DictReader(open(res_f)))
ver = list(csv.DictReader(open(ver_f)))
for r in res:
    r["tm"] = (float(r["t_inicio"]) + float(r["t_fin"])) / 2
    r["x"] = float(r["x_medio_m"]); r["v"] = float(r["kmh"])
libres = list(res); pares = []; sin = []
for v in ver:
    tv, xv, vv = float(v["t_medio_s"]), float(v["carril_x_m"]), float(v["v_kmh"])
    cand = [r for r in libres if abs(r["x"] - xv) < 1.2]
    if not cand: sin.append(v["id"]); continue
    r = min(cand, key=lambda r: abs(r["tm"] - tv))
    if abs(r["tm"] - tv) > 3.0: sin.append(v["id"]); continue
    libres.remove(r); pares.append((v["id"], vv, r["v"], r["v"] - vv))
print(f"{'id':>4} {'real km/h':>10} {'medida km/h':>12} {'error':>8}")
for i, a, b, e in pares: print(f"{i:>4} {a:>10.1f} {b:>12.2f} {e:>+8.2f}")
errs = [abs(e) for *_, e in pares]
print(f"\nvehículos simulados: {len(ver)}   detectados: {len(pares)}   sin detectar: {len(sin)} {sin}   pistas de más: {len(libres)}")
if errs:
    print(f"error medio: {st.mean(errs):.2f} km/h   máximo: {max(errs):.2f} km/h   (requisito: ±5 km/h)")
    print("RESULTADO:", "CUMPLE" if max(errs) <= 5 and not sin and not libres else "REVISAR")
