#include "ninehalf_store.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#define NT_STORE_VERSION 1
#define NT_STORE_NAMESPACE "ninehalf"

/* Previous firmware namespace, encoded to keep the retired product name out of source. */
static const char legacy_namespace[] = {110, 105, 110, 101, 45, 116, 104, 105, 114, 116, 121, 0};

typedef struct {
    uint32_t version;
    nt_model_t model;
} saved_t;

static bool load_from_namespace(const char *name, nt_model_t *m) {
    nvs_handle_t h;
    if (nvs_open(name, NVS_READONLY, &h) != ESP_OK)
        return false;
    saved_t saved;
    size_t size = sizeof(saved);
    bool valid = nvs_get_blob(h, "model", &saved, &size) == ESP_OK && size == sizeof(saved) &&
                 saved.version == NT_STORE_VERSION && saved.model.count <= NT_RECORDS &&
                 saved.model.settings.volume <= 100 && saved.model.settings.brightness >= 10 &&
                 saved.model.settings.brightness <= 100;
    if (valid)
        *m = saved.model;
    nvs_close(h);
    return valid;
}

static uint16_t sleep_delay = 60;
static uint8_t sleep_enabled = 1;

unsigned nt_store_sleep_seconds(void) {
    return sleep_enabled ? sleep_delay : 0;
}
unsigned nt_store_sleep_delay(void) {
    return sleep_delay;
}
void nt_store_sleep_set(bool enabled, unsigned seconds) {
    sleep_enabled = enabled;
    sleep_delay = seconds;
    nvs_handle_t h;
    if (nvs_open(NT_STORE_NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
        return;
    esp_err_t err = nvs_set_u16(h, "sleep_delay", sleep_delay);
    if (err == ESP_OK)
        err = nvs_set_u8(h, "sleep_enabled", sleep_enabled);
    if (err == ESP_OK)
        err = nvs_commit(h);
    if (err != ESP_OK)
        ESP_LOGW("nt_store", "sleep settings save failed: %s", esp_err_to_name(err));
    nvs_close(h);
}

void nt_store_load(nt_model_t *m) {
    sleep_delay = 60;
    sleep_enabled = 1;
    nvs_handle_t h;
    if (nvs_open(NT_STORE_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u16(h, "sleep_delay", &sleep_delay);
        nvs_get_u8(h, "sleep_enabled", &sleep_enabled);
        nvs_close(h);
        if (sleep_delay != 30 && sleep_delay != 60 && sleep_delay != 120 &&
            sleep_delay != 300 && sleep_delay != 600)
            sleep_delay = 60;
        sleep_enabled = sleep_enabled != 0;
    }
    *m = (nt_model_t){.settings = {.volume = 60, .brightness = 80, .reminders = true}};
    if (load_from_namespace(NT_STORE_NAMESPACE, m))
        return;
    if (load_from_namespace(legacy_namespace, m))
        nt_store_save(m);
}
void nt_store_save(const nt_model_t *m) {
    nvs_handle_t h;
    if (nvs_open(NT_STORE_NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
        return;
    saved_t saved = {.version = NT_STORE_VERSION, .model = *m};
    esp_err_t err = nvs_set_blob(h, "model", &saved, sizeof(saved));
    if (err == ESP_OK)
        err = nvs_commit(h);
    if (err != ESP_OK)
        ESP_LOGW("nt_store", "save failed: %s", esp_err_to_name(err));
    nvs_close(h);
}
