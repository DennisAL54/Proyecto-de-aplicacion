// main.cpp del ensayo en vivo (10.8). El main del ensayo 10.7 queda como main_ensayo107.txt.bak
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_storage.h"
#include "vivo.h"

static const char *TAG = "main";
#define DURACION_S 120            // segundos de medición

static void tarea_vivo(void *)
{
    if (sd_mount() != ESP_OK) {
        ESP_LOGE(TAG, "no monta la SD");
    } else {
        vivo_ejecutar(DURACION_S);
        sd_unmount();
        ESP_LOGI(TAG, "SD desmontada: ya puede sacar la tarjeta");
    }
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    xTaskCreate(tarea_vivo, "vivo", 8192, NULL, 5, NULL);
}
