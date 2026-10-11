#include "ninehalf_ui.h"
#include "ninehalf_network.h"
#include "ninehalf_store.h"
#include "bsp_display.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "demo_radio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int kind;
    bsp_btn_t key;
    bsp_btn_ev_t event;
    nt_model_t *snapshot;
} message_t;
static QueueHandle_t events;
static nt_model_t model;
static void button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG)
        return;
    message_t m = {.kind = 1, .key = btn, .event = ev};
    xQueueSend(events, &m, 0);
}
static void network_result(const nt_model_t *snapshot, int kind) {
    message_t m = {.kind = kind};
    if (snapshot) {
        m.snapshot = malloc(sizeof(*snapshot));
        if (!m.snapshot)
            return;
        *m.snapshot = *snapshot;
    }
    if (xQueueSend(events, &m, pdMS_TO_TICKS(100)) != pdTRUE)
        free(m.snapshot);
}
void nt_app_network_result(const nt_model_t *snapshot, bool success) {
    network_result(snapshot, success ? 2 : 3);
}
void nt_app_history_result(const nt_model_t *snapshot) {
    network_result(snapshot, 4);
}
void nt_app_sync_started(void) {
    network_result(NULL, 5);
}
void nt_app_sync_finished(bool success) {
    network_result(NULL, success ? 6 : 3);
}
static void chime(void *arg) {
    (void)arg;
    int16_t samples[160];
    if (bsp_audio_init() != ESP_OK || bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
        vTaskDelete(NULL);
        return;
    }
    for (int block = 0; block < 20; block++) {
        for (int i = 0; i < 160; i++)
            samples[i] = (int16_t)(sin((block * 160 + i) * 2 * 3.141592653589793 * 880 / 16000) *
                                   4000 * (20 - block) / 20);
        bsp_audio_write(samples, sizeof(samples));
    }
    vTaskDelete(NULL);
}
static void allocation_failed(size_t size, uint32_t caps, const char *function) {
    esp_rom_printf("Allocation failed: %s bytes=%u caps=0x%x\n", function, (unsigned)size,
                   (unsigned)caps);
}
void app_main(void) {
    ESP_ERROR_CHECK(heap_caps_register_failed_alloc_callback(allocation_failed));
    ESP_ERROR_CHECK(demo_radio_nvs_prepare());
    nt_store_load(&model);
    ESP_ERROR_CHECK(bsp_display_init());
    if (!bsp_lvgl_init())
        return;
    bsp_battery_init();
    bsp_audio_set_volume(model.settings.volume);
    bsp_display_backlight(model.settings.brightness);
    events = xQueueCreate(4, sizeof(message_t));
    if (!events)
        return;
    if (bsp_lvgl_lock(-1)) {
        nt_ui_create(&model);
        bsp_lvgl_unlock();
    }
    ESP_ERROR_CHECK(bsp_button_init(button, NULL));
    nt_network_start();
    message_t *m = malloc(sizeof(*m));
    if (!m)
        return;
    bool initial = true;
    for (;;) {
        if (xQueueReceive(events, m, pdMS_TO_TICKS(1000))) {
            if (m->kind == 1) {
                if (bsp_lvgl_lock(-1)) {
                    nt_ui_key(m->key, m->event);
                    bsp_lvgl_unlock();
                }
            } else if (m->kind == 5 || m->kind == 6) {
                if (bsp_lvgl_lock(-1)) {
                    if (m->kind == 5)
                        nt_ui_sync_begin();
                    else
                        nt_ui_sync_state(true);
                    nt_ui_render();
                    bsp_lvgl_unlock();
                }
            } else if (m->kind == 3) {
                if (bsp_lvgl_lock(-1)) {
                    nt_ui_sync_state(false);
                    nt_ui_render();
                    bsp_lvgl_unlock();
                }
            } else if (m->kind == 2 || m->kind == 4) {
                bool changed = false, notify = false;
                if (m->snapshot && m->snapshot->count) {
                    nt_model_t before = model;
                    nt_merge_records(&model, m->snapshot->records, m->snapshot->count);
                    changed = memcmp(before.records, model.records, sizeof(model.records)) != 0 ||
                              before.count != model.count;
                    if (initial && m->kind == 2) {
                        if (!model.settings.notified[0]) {
                            strcpy(model.settings.notified, model.records[0].issue);
                            changed = true;
                        }
                        initial = false;
                    } else if (m->kind == 2 &&
                               strcmp(model.settings.notified, model.records[0].issue) &&
                               strcmp(before.records[0].issue, model.records[0].issue)) {
                        notify = model.settings.reminders;
                        strcpy(model.settings.notified, model.records[0].issue);
                        changed = true;
                    }
                    if (changed)
                        nt_store_save(&model);
                    if (notify) {
                        if (bsp_lvgl_lock(-1)) {
                            nt_ui_notify();
                            bsp_lvgl_unlock();
                        }
                        xTaskCreate(chime, "nt_chime", 3072, NULL, 3, NULL);
                    }
                }
                if (bsp_lvgl_lock(-1)) {
                    nt_ui_render();
                    bsp_lvgl_unlock();
                }
            }
            free(m->snapshot);
            m->snapshot = NULL;
        }
        if (bsp_lvgl_lock(-1)) {
            static bool online;
            bool current = nt_wifi_online();
            if (current != online) {
                online = current;
                nt_ui_render();
            }
            nt_ui_clock();
            bsp_lvgl_unlock();
        }
    }
}
