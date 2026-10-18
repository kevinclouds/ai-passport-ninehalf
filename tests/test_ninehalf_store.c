#include "ninehalf_store.h"
#include "nvs.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t version;
    nt_model_t model;
} saved_t;

static saved_t old_slot;
static saved_t new_slot;
static bool has_old;
static bool has_new;
static int writes;

esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    (void)mode;
    if (strcmp(name, "ninehalf") == 0)
        *handle = 1;
    else
        *handle = 2;
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *value, size_t *size) {
    assert(strcmp(key, "model") == 0);
    saved_t *slot = handle == 1 ? &new_slot : &old_slot;
    if (!(handle == 1 ? has_new : has_old) || *size < sizeof(*slot))
        return -1;
    memcpy(value, slot, sizeof(*slot));
    *size = sizeof(*slot);
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t size) {
    assert(handle == 1 && strcmp(key, "model") == 0 && size == sizeof(new_slot));
    memcpy(&new_slot, value, size);
    has_new = true;
    writes++;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle) {
    assert(handle == 1);
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle) {
    assert(handle == 1 || handle == 2);
}

const char *esp_err_to_name(esp_err_t error) {
    (void)error;
    return "error";
}

static bool has_sleep;
static uint16_t saved_delay;
static uint8_t saved_enabled;
esp_err_t nvs_get_u16(nvs_handle_t h, const char *key, uint16_t *v) {
    assert(h == 1 && !strcmp(key, "sleep_delay"));
    if (!has_sleep) return -1;
    *v = saved_delay;
    return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *v) {
    assert(h == 1 && !strcmp(key, "sleep_enabled"));
    if (!has_sleep) return -1;
    *v = saved_enabled;
    return ESP_OK;
}
esp_err_t nvs_set_u16(nvs_handle_t h, const char *key, uint16_t v) {
    assert(h == 1 && !strcmp(key, "sleep_delay"));
    saved_delay = v;
    return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v) {
    assert(h == 1 && !strcmp(key, "sleep_enabled"));
    saved_enabled = v;
    has_sleep = true;
    return ESP_OK;
}

int main(void) {
    nt_model_t loaded;
    nt_store_load(&loaded);
    assert(loaded.count == 0 && loaded.settings.volume == 60 && writes == 0);

    assert(nt_store_sleep_seconds() == 60);
    nt_store_sleep_set(false, 300);
    nt_store_load(&loaded);
    assert(nt_store_sleep_seconds() == 0 && nt_store_sleep_delay() == 300);
    nt_store_sleep_set(true, 120);
    nt_store_load(&loaded);
    assert(nt_store_sleep_seconds() == 120);
    saved_delay = 99;
    nt_store_load(&loaded);
    assert(nt_store_sleep_seconds() == 60);

    old_slot.version = 1;
    old_slot.model.settings.volume = 42;
    old_slot.model.settings.brightness = 70;
    old_slot.model.count = 1;
    has_old = true;
    nt_store_load(&loaded);
    assert(loaded.count == 1 && loaded.settings.volume == 42);
    assert(has_new && new_slot.model.settings.volume == 42 && writes == 1);

    new_slot.model.settings.volume = 77;
    nt_store_load(&loaded);
    assert(loaded.settings.volume == 77 && writes == 1);

    new_slot.version = 99;
    nt_store_load(&loaded);
    assert(loaded.settings.volume == 42 && new_slot.version == 1 && writes == 2);
    return 0;
}
