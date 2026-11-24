#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "sd_storage.h"

#define SD_PIN_CLK  39
#define SD_PIN_CMD  38
#define SD_PIN_D0   40

static const char *TAG = "sd";
static sdmmc_card_t *card = nullptr;
static char ruta_actual[64];
static char nombre_actual[40];

esp_err_t sd_mount(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {};
    mount_cfg.format_if_mount_failed = false;      // nunca formatear sola
    mount_cfg.max_files = 4;
    mount_cfg.allocation_unit_size = 16 * 1024;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;             // bus de 1 bit

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = (gpio_num_t)SD_PIN_CLK;
    slot.cmd = (gpio_num_t)SD_PIN_CMD;
    slot.d0  = (gpio_num_t)SD_PIN_D0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t r = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount_cfg, &card);
    if (r == ESP_OK) sdmmc_card_print_info(stdout, card);
    else ESP_LOGE(TAG, "no se pudo montar: %s (0x%x)", esp_err_to_name(r), (unsigned)r);
    return r;
}

void sd_unmount(void)
{
    if (card) { esp_vfs_fat_sdcard_unmount("/sdcard", card); card = nullptr; }
}

static void iso8601_ahora(char *out, size_t n)
{
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%dT%H:%M:%S", &tm);
}

bool sd_nuevo_archivo(void)
{
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(nombre_actual, sizeof(nombre_actual), "datos_%Y%m%d_%H%M%S.csv", &tm);
    snprintf(ruta_actual, sizeof(ruta_actual), "/sdcard/%s", nombre_actual);

    FILE *f = fopen(ruta_actual, "w");
    if (!f) return false;
    fprintf(f, "ID_tramo,Longitud_tramo,ID_Vehiculo,Velocidad_kmh,Fecha_Hora,Codigo_Error\n");
    bool ok = (fflush(f) == 0) && (fsync(fileno(f)) == 0);
    fclose(f);
    return ok;
}

const char *sd_nombre_actual(void) { return nombre_actual; }

bool sd_registro_vehiculo(int id_tramo, float longitud, int id_veh, float vel_kmh)
{
    char ts[24];
    iso8601_ahora(ts, sizeof(ts));
    FILE *f = fopen(ruta_actual, "a");
    if (!f) return false;
    int n = fprintf(f, "%d,%.1f,%d,%.1f,%s,\n", id_tramo, longitud, id_veh, vel_kmh, ts);
    bool ok = (n > 0) && (fflush(f) == 0) && (fsync(fileno(f)) == 0);
    fclose(f);
    return ok;
}

float sd_porcentaje_usado(void)
{
    uint64_t total = 0, libre = 0;
    if (esp_vfs_fat_info("/sdcard", &total, &libre) != ESP_OK || total == 0) return -1;
    return 100.0f * (float)(total - libre) / (float)total;
}

// Los nombres llevan fecha y hora: el menor alfabéticamente es el más antiguo.
bool sd_borrar_mas_antiguo(const char *excluir)
{
    DIR *d = opendir("/sdcard");
    if (!d) return false;
    char viejo[260] = "";
    struct dirent *e;
    while ((e = readdir(d)) != nullptr) {
        if (strncmp(e->d_name, "datos_", 6) != 0) continue;
        if (excluir && strcmp(e->d_name, excluir) == 0) continue;
        if (viejo[0] == 0 || strcmp(e->d_name, viejo) < 0) strncpy(viejo, e->d_name, sizeof(viejo) - 1);
    }
    closedir(d);
    if (viejo[0] == 0) return false;
    char ruta[300];
    snprintf(ruta, sizeof(ruta), "/sdcard/%s", viejo);
    ESP_LOGI(TAG, "borrando el más antiguo: %s", viejo);
    return unlink(ruta) == 0;
}

void sd_listar(void)
{
    DIR *d = opendir("/sdcard");
    if (!d) { ESP_LOGE(TAG, "no se pudo abrir /sdcard"); return; }
    struct dirent *e;
    while ((e = readdir(d)) != nullptr) {
        char ruta[300];
        snprintf(ruta, sizeof(ruta), "/sdcard/%s", e->d_name);
        struct stat st;
        if (stat(ruta, &st) == 0) ESP_LOGI(TAG, "  %-30s %ld bytes", e->d_name, (long)st.st_size);
    }
    closedir(d);
}
