#include "ia_yolo.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_yolo.h"
#include "yolo_det.h"

static const char *TAG = "yolo";

static uint8_t *g_arena = nullptr;
static tflite::MicroMutableOpResolver<9> *g_ops = nullptr;
static tflite::MicroInterpreter *g_interp = nullptr;

bool yolo_iniciar(size_t arena_bytes)
{
    const tflite::Model *model = tflite::GetModel(g_yolo_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "versión de esquema %lu distinta de %d", (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    g_arena = (uint8_t *)heap_caps_aligned_alloc(16, arena_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_arena) { ESP_LOGE(TAG, "no hay PSRAM para el arena de %u bytes", (unsigned)arena_bytes); return false; }

    g_ops = new tflite::MicroMutableOpResolver<9>();
    g_ops->AddPad();             g_ops->AddConv2D();        g_ops->AddDepthwiseConv2D();
    g_ops->AddLeakyRelu();       g_ops->AddAdd();           g_ops->AddMaxPool2D();
    g_ops->AddConcatenation();   g_ops->AddResizeNearestNeighbor();
    g_ops->AddQuantize();        // por si el conversor dejó alguna (no pasa nada si no se usa)

    g_interp = new tflite::MicroInterpreter(model, *g_ops, g_arena, arena_bytes);
    if (g_interp->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors falló (arena pequeño u operación sin registrar; mire los mensajes de arriba)");
        yolo_terminar();
        return false;
    }
    TfLiteTensor *in = g_interp->input(0);
    ESP_LOGI(TAG, "modelo %u bytes | entrada %dx%dx%d tipo %d | arena usada %u de %u bytes",
             g_yolo_model_len, in->dims->data[1], in->dims->data[2], in->dims->data[3], (int)in->type,
             (unsigned)g_interp->arena_used_bytes(), (unsigned)arena_bytes);
    if (g_interp->outputs_size() != 2) { ESP_LOGE(TAG, "se esperaban 2 salidas y hay %u", (unsigned)g_interp->outputs_size()); yolo_terminar(); return false; }
    return true;
}

void yolo_terminar(void)
{
    delete g_interp; g_interp = nullptr;
    delete g_ops;    g_ops = nullptr;
    if (g_arena) { heap_caps_free(g_arena); g_arena = nullptr; }
}

// Lee un BMP de 24 bits sin compresión. Devuelve un buffer RGB888 (arriba->abajo) en PSRAM, o nullptr.
static uint8_t *leer_bmp(const char *ruta, int *w, int *h)
{
    FILE *f = fopen(ruta, "rb");
    if (!f) { ESP_LOGE(TAG, "no se pudo abrir %s", ruta); return nullptr; }
    uint8_t cab[54];
    if (fread(cab, 1, 54, f) != 54 || cab[0] != 'B' || cab[1] != 'M') { ESP_LOGE(TAG, "%s no es un BMP", ruta); fclose(f); return nullptr; }
    auto le32 = [&](int o) { return (int32_t)(cab[o] | (cab[o+1] << 8) | (cab[o+2] << 16) | ((uint32_t)cab[o+3] << 24)); };
    int32_t off = le32(10), W = le32(18), H = le32(22);
    int bpp = cab[28] | (cab[29] << 8);
    bool abajo_arriba = H > 0; if (H < 0) H = -H;
    if (bpp != 24 || le32(30) != 0 || W <= 0 || W > YOLO_TAM || H <= 0 || H > YOLO_TAM) {
        ESP_LOGE(TAG, "BMP no soportado (%dx%d, %d bpp): se necesita 24 bits sin compresión, máx 320x320", (int)W, (int)H, bpp);
        fclose(f); return nullptr;
    }
    uint8_t *rgb = (uint8_t *)heap_caps_malloc((size_t)W * H * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *fila = (uint8_t *)heap_caps_malloc(((W * 3 + 3) & ~3), MALLOC_CAP_8BIT);
    if (!rgb || !fila) { free(rgb); free(fila); fclose(f); return nullptr; }
    fseek(f, off, SEEK_SET);
    size_t paso = (size_t)((W * 3 + 3) & ~3);
    for (int r = 0; r < H; r++) {
        if (fread(fila, 1, paso, f) != paso) { ESP_LOGE(TAG, "BMP incompleto"); free(rgb); free(fila); fclose(f); return nullptr; }
        int y = abajo_arriba ? H - 1 - r : r;
        uint8_t *d = rgb + (size_t)y * W * 3;
        for (int x = 0; x < W; x++) { d[3*x] = fila[3*x+2]; d[3*x+1] = fila[3*x+1]; d[3*x+2] = fila[3*x]; }   // BGR -> RGB
    }
    free(fila); fclose(f);
    *w = W; *h = H;
    return rgb;
}

int yolo_probar_bmp(const char *ruta, float umbral)
{
    if (!g_interp) return -1;
    int w, h;
    uint8_t *rgb = leer_bmp(ruta, &w, &h);
    if (!rgb) return -1;

    TfLiteTensor *in = g_interp->input(0);
    int ox, oy;
    yolo_entrada_rgb888(rgb, w, h, in->data.int8, &ox, &oy);
    heap_caps_free(rgb);

    int64_t t0 = esp_timer_get_time();
    TfLiteStatus st = g_interp->Invoke();
    int64_t dt = esp_timer_get_time() - t0;
    if (st != kTfLiteOk) { ESP_LOGE(TAG, "Invoke falló"); return -1; }
    ESP_LOGI(TAG, "%s: inferencia %lld ms", ruta, (long long)(dt / 1000));

    // Las dos salidas pueden venir en cualquier orden: se distinguen por su tamaño (10x10 o 20x20)
    TfLiteTensor *o0 = g_interp->output(0), *o1 = g_interp->output(1);
    TfLiteTensor *c10 = (o0->dims->data[1] == 10) ? o0 : o1;
    TfLiteTensor *c20 = (o0->dims->data[1] == 10) ? o1 : o0;
    YoloSalida s10 = { c10->data.int8, c10->params.scale, c10->params.zero_point };
    YoloSalida s20 = { c20->data.int8, c20->params.scale, c20->params.zero_point };

    YoloCaja cajas[YOLO_MAX_CAJAS];
    int n = yolo_decodificar(s10, s20, umbral, true, cajas, YOLO_MAX_CAJAS);
    ESP_LOGI(TAG, "%d cajas de vehículo (umbral %.2f):", n, umbral);
    for (int i = 0; i < n; i++)
        ESP_LOGI(TAG, "  clase %d conf %.2f caja %.1f %.1f %.1f %.1f", cajas[i].clase, cajas[i].conf,
                 cajas[i].x0, cajas[i].y0, cajas[i].x1, cajas[i].y1);
    return n;
}
