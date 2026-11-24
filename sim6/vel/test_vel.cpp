#include <stdio.h>
#include <math.h>
#include <random>
#include "velocidad.h"

static int fallos = 0;
static void verificar(const char *nombre, bool ok, const char *detalle)
{
    printf("   %-52s %s  %s\n", nombre, ok ? "OK   " : "FALLA", detalle);
    if (!ok) fallos++;
}

int main()
{
    const double FPS = 7.0;
    std::mt19937 gen(7);
    std::normal_distribution<double> ruido(0.0, 0.15);       // 15 cm de ruido en la posición
    Muestra m[VEL_MAX_MUESTRAS];
    double kmh;
    int usadas;
    char buf[96];

    printf("1) Trayectoria perfecta a 40 km/h por el carril X = 1.75 m (24 m de tramo, 7 FPS)\n");
    int n = 0;
    for (double y = 6.0; y <= 30.0 && n < 40; y += (40 / 3.6) / FPS, n++) m[n] = {n / FPS, 1.75, y};
    bool ok = vel_ajustar(m, n, &kmh, &usadas);
    snprintf(buf, sizeof buf, "calculada %.3f km/h con %d muestras (esperada 40)", kmh, usadas);
    verificar("velocidad = 40 km/h (error < 0.01)", ok && fabs(kmh - 40.0) < 0.01 && usadas == n, buf);

    printf("2) La misma trayectoria con ruido de 15 cm\n");
    for (int i = 0; i < n; i++) { m[i].x += ruido(gen); m[i].y += ruido(gen); }
    ok = vel_ajustar(m, n, &kmh, &usadas);
    snprintf(buf, sizeof buf, "calculada %.2f km/h con %d muestras", kmh, usadas);
    verificar("velocidad dentro de ±2 km/h de 40", ok && fabs(kmh - 40.0) < 2.0, buf);

    printf("3) Con un valor atípico (una muestra desplazada 5 m)\n");
    m[n / 2].y += 5.0;
    ok = vel_ajustar(m, n, &kmh, &usadas);
    snprintf(buf, sizeof buf, "calculada %.2f km/h, usó %d de %d muestras", kmh, usadas, n);
    verificar("el atípico se descarta (usó n-1) y el error es < 2 km/h", ok && usadas == n - 1 && fabs(kmh - 40.0) < 2.0, buf);

    printf("4) Movimiento en diagonal: vx = 2 m/s, vy = 8 m/s\n");
    for (int i = 0; i < 10; i++) m[i] = {i / FPS, 1.0 + 2.0 * i / FPS, 6.0 + 8.0 * i / FPS};
    ok = vel_ajustar(m, 10, &kmh, &usadas);
    double esperada = hypot(2.0, 8.0) * 3.6;
    snprintf(buf, sizeof buf, "calculada %.3f km/h (esperada %.3f)", kmh, esperada);
    verificar("usa las dos coordenadas", ok && fabs(kmh - esperada) < 0.01, buf);

    printf("5) Vehículo que se acerca (y decrece) a 55 km/h, con marcas de tiempo grandes\n");
    for (int i = 0; i < 12; i++) m[i] = {100000.0 + i / FPS, 5.25, 30.0 - (55 / 3.6) * i / FPS};
    ok = vel_ajustar(m, 12, &kmh, &usadas);
    snprintf(buf, sizeof buf, "calculada %.3f km/h (esperada 55)", kmh);
    verificar("la velocidad es siempre positiva y exacta", ok && fabs(kmh - 55.0) < 0.01, buf);

    printf("6) Casos inválidos\n");
    verificar("una sola muestra devuelve false", !vel_ajustar(m, 1, &kmh, &usadas), "");
    Muestra iguales[3] = {{1, 0, 0}, {1, 1, 1}, {1, 2, 2}};
    verificar("tiempos iguales devuelven false", !vel_ajustar(iguales, 3, &kmh, &usadas), "");

    printf("\n%s\n", fallos == 0 ? "TODAS LAS PRUEBAS PASARON" : "HAY PRUEBAS FALLIDAS");
    return fallos ? 1 : 0;
}
