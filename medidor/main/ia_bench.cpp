#include <stdint.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "ia_bench.h"

extern const unsigned char g_model[];
extern const unsigned int g_model_len;

static const char *TAG = "ia";

bool ia_benchmark(size_t arena_bytes, uint32_t caps, int repeticiones)
{
    const tflite::Model *model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "versión de esquema %lu distinta de %d",
                 (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    ESP_LOGI(TAG, "modelo: %u bytes", (unsigned)g_model_len);

    uint8_t *arena = (uint8_t *)heap_caps_aligned_alloc(16, arena_bytes, caps);
    if (!arena) {
        ESP_LOGE(TAG, "no hay memoria para un arena de %u bytes", (unsigned)arena_bytes);
        return false;
    }

    bool ok = false;
    {   // el intérprete debe destruirse antes de liberar el arena
        tflite::MicroMutableOpResolver<12> ops;
        ops.AddConv2D();         ops.AddDepthwiseConv2D();  ops.AddAdd();
        ops.AddPad();            ops.AddMean();             ops.AddReshape();
        ops.AddFullyConnected(); ops.AddSoftmax();          ops.AddQuantize();
        ops.AddDequantize();     ops.AddAveragePool2D();    ops.AddMaxPool2D();

        tflite::MicroInterpreter interp(model, ops, arena, arena_bytes);
        if (interp.AllocateTensors() != kTfLiteOk) {
            ESP_LOGE(TAG, "AllocateTensors falló (¿arena pequeño u operación sin registrar?)");
        } else {
            TfLiteTensor *in = interp.input(0);
            ESP_LOGI(TAG, "entrada %dx%dx%d, tipo %d | arena usada %u de %u bytes",
                     in->dims->data[1], in->dims->data[2], in->dims->data[3], (int)in->type,
                     (unsigned)interp.arena_used_bytes(), (unsigned)arena_bytes);

            for (size_t i = 0; i < in->bytes; i++) {
                int v = (int)((i * 37) % 255) - 127;
                in->data.int8[i] = (int8_t)v;
            }

            int64_t t0 = esp_timer_get_time();
            TfLiteStatus st = interp.Invoke();
            ESP_LOGI(TAG, "1a inferencia (en frío): %lld ms (%s)",
                     (long long)((esp_timer_get_time() - t0) / 1000), st == kTfLiteOk ? "ok" : "ERROR");

            int64_t suma = 0, minimo = INT64_MAX, maximo = 0;
            for (int i = 0; i < repeticiones; i++) {
                int64_t a = esp_timer_get_time();
                interp.Invoke();
                int64_t d = esp_timer_get_time() - a;
                suma += d;
                if (d < minimo) minimo = d;
                if (d > maximo) maximo = d;
                vTaskDelay(pdMS_TO_TICKS(10));       // cede el procesador (evita el watchdog)
            }
            double prom_ms = suma / 1000.0 / repeticiones;
            ESP_LOGI(TAG, "inferencia: promedio %.1f ms, mín %lld ms, máx %lld ms -> %.1f FPS",
                     prom_ms, (long long)(minimo / 1000), (long long)(maximo / 1000), 1000.0 / prom_ms);
            ok = true;
        }
    }
    heap_caps_free(arena);
    return ok;
}
