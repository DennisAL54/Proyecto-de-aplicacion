#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"

#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D7     16   // Y9
#define CAM_PIN_D6     17   // Y8
#define CAM_PIN_D5     18   // Y7
#define CAM_PIN_D4     12   // Y6
#define CAM_PIN_D3     10   // Y5
#define CAM_PIN_D2      8   // Y4
#define CAM_PIN_D1      9   // Y3
#define CAM_PIN_D0     11   // Y2
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK   13

static const char *TAG = "camara";

esp_err_t camera_init(void)
{
    camera_config_t cfg = {};
    cfg.pin_pwdn = CAM_PIN_PWDN;   cfg.pin_reset = CAM_PIN_RESET;
    cfg.pin_xclk = CAM_PIN_XCLK;
    cfg.pin_sccb_sda = CAM_PIN_SIOD;  cfg.pin_sccb_scl = CAM_PIN_SIOC;
    cfg.pin_d7 = CAM_PIN_D7; cfg.pin_d6 = CAM_PIN_D6; cfg.pin_d5 = CAM_PIN_D5; cfg.pin_d4 = CAM_PIN_D4;
    cfg.pin_d3 = CAM_PIN_D3; cfg.pin_d2 = CAM_PIN_D2; cfg.pin_d1 = CAM_PIN_D1; cfg.pin_d0 = CAM_PIN_D0;
    cfg.pin_vsync = CAM_PIN_VSYNC; cfg.pin_href = CAM_PIN_HREF; cfg.pin_pclk = CAM_PIN_PCLK;

    cfg.xclk_freq_hz = 20000000;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.ledc_channel = LEDC_CHANNEL_0;

    cfg.pixel_format = PIXFORMAT_RGB565;
    cfg.frame_size   = FRAMESIZE_QVGA;
    cfg.jpeg_quality = 12;
    cfg.fb_count     = 2;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode    = CAMERA_GRAB_LATEST;
    return esp_camera_init(&cfg);
}

void camera_benchmark(void)
{
    const int N = 100;
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < N; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) { ESP_LOGE(TAG, "no llegó cuadro"); continue; }
        if (i == 0) ESP_LOGI(TAG, "cuadro %dx%d, %u bytes", fb->width, fb->height, (unsigned)fb->len);
        esp_camera_fb_return(fb);
    }
    float seg = (esp_timer_get_time() - t0) / 1e6f;
    ESP_LOGI(TAG, "FPS de captura: %.1f", N / seg);
}
