#pragma once
#include "esp_camera.h"

bool guardar_bmp(const camera_fb_t *fb, const char *ruta);
bool guardar_jpeg(camera_fb_t *fb, const char *ruta);
