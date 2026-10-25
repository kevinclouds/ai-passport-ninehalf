#include "ninehalf_ui.h"
#include "ninehalf_network.h"
#include "ninehalf_store.h"
#include "ninehalf_power.h"
#include "bsp_display.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "demo_radio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    int kind;
    bsp_btn_t key;
    bsp_btn_ev_t event;
    nt_model_t *snapshot;
    nt_live_t live;
} message_t;
static QueueHandle_t events;
static nt_model_t model;
static SemaphoreHandle_t audio_lock;
static void button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (ev != BSP_BTN_PRESS && ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG)
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
void nt_app_history_sync_started(void) {
    network_result(NULL, 8);
}
void nt_app_history_sync_finished(void) {
    network_result(NULL, 9);
}
void nt_app_live_result(const nt_live_t *live) {
    message_t m = {.kind = 7, .live = *live};
    xQueueSend(events, &m, pdMS_TO_TICKS(100));
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
    xSemaphoreTake(audio_lock, portMAX_DELAY);
    nt_power_busy(true);
    bsp_audio_set_volume(model.settings.volume);
    if (bsp_audio_init() != ESP_OK || bsp_audio_wake() != ESP_OK ||
        bsp_audio_set_format(16000, 16, 1) != ESP_OK)
        goto done;
    for (int block = 0; block < 20; block++) {
        for (int i = 0; i < 160; i++)
            samples[i] = (int16_t)(sin((block * 160 + i) * 2 * 3.141592653589793 * 880 / 16000) *
                                   4000 * (20 - block) / 20);
        bsp_audio_write(samples, sizeof(samples));
    }
done:
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_audio_sleep());
    nt_power_busy(false);
    xSemaphoreGive(audio_lock);
    vTaskDelete(NULL);
}
static void allocation_failed(size_t size, uint32_t caps, const char *function) {
    esp_rom_printf("Allocation failed: %s bytes=%u caps=0x%x\n", function, (unsigned)size,
                   (unsigned)caps);
}
void app_main(void) {
    ESP_ERROR_CHECK(heap_caps_register_failed_alloc_callback(allocation_failed));
    nt_power_init();
    nt_power_busy(true);
    ESP_ERROR_CHECK(demo_radio_nvs_prepare());
    nt_store_load(&model);
    ESP_ERROR_CHECK(bsp_display_init());
    if (!bsp_lvgl_init())
        return;
    bsp_battery_init();
    bsp_audio_set_volume(model.settings.volume);
    audio_lock = xSemaphoreCreateMutex();
    if (!audio_lock)
        return;
    // The codec may consume power even before the first reminder is played.
    if (bsp_audio_init() == ESP_OK)
        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_audio_sleep());
    bsp_display_backlight(model.settings.brightness);
    events = xQueueCreate(8, sizeof(message_t));
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
    nt_live_t current_live = {0};
    int64_t last_input_us = esp_timer_get_time(), woke_at_us = 0;
    bsp_btn_t wake_key = BSP_BTN_OK;
    bool screen_off = false;
    for (;;) {
        if (xQueueReceive(events, m, screen_off ? portMAX_DELAY : pdMS_TO_TICKS(1000))) {
            if (m->kind == 1) {
                int64_t now_us = esp_timer_get_time();
                last_input_us = now_us;
                if (m->event == BSP_BTN_PRESS && screen_off) {
                    woke_at_us = now_us;
                    wake_key = m->key;
                    nt_power_busy(true);
                    if (bsp_lvgl_lock(-1)) {
                        esp_err_t err = bsp_display_sleep(false);
                        if (err == ESP_OK) {
                            nt_ui_sleep(false);
                            nt_ui_render();
                            err = bsp_lvgl_refr_now();
                            if (err == ESP_OK)
                                err = bsp_lvgl_sleep(false);
                            if (err == ESP_OK) {
                                bsp_display_backlight(model.settings.brightness);
                                screen_off = false;
                            } else
                                nt_ui_sleep(true);
                        }
                        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
                        bsp_lvgl_unlock();
                    }
                    if (screen_off)
                        nt_power_busy(false);
                    else
                        nt_network_display(true);
                } else if (m->event != BSP_BTN_PRESS &&
                           !(woke_at_us && m->key == wake_key &&
                             now_us - woke_at_us < 2000000) && bsp_lvgl_lock(-1)) {
                    nt_ui_key(m->key, m->event);
                    bsp_lvgl_unlock();
                }
                if (m->event != BSP_BTN_PRESS && m->key == wake_key)
                    woke_at_us = 0;
            } else if (m->kind == 5 || m->kind == 6) {
                if (bsp_lvgl_lock(-1)) {
                    if (m->kind == 5)
                        nt_ui_sync_begin();
                    else
                        nt_ui_sync_state(true);
                    bsp_lvgl_unlock();
                }
            } else if (m->kind == 3) {
                if (bsp_lvgl_lock(-1)) {
                    nt_ui_sync_state(false);
                    bsp_lvgl_unlock();
                }
            } else if (m->kind == 8 || m->kind == 9) {
                if (bsp_lvgl_lock(-1)) {
                    nt_ui_history_sync_state(m->kind == 8);
                    bsp_lvgl_unlock();
                }
            } else if (m->kind == 7) {
                if (nt_live_merge(&current_live, &m->live)) {
                    initial = false;
                    if (bsp_lvgl_lock(-1)) {
                        nt_ui_set_live(&current_live);
                        bsp_lvgl_unlock();
                    }
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
                    if (current_live.present && model.count &&
                        strcmp(model.records[0].issue, current_live.record.issue) >= 0) {
                        current_live = (nt_live_t){0};
                        nt_ui_set_live(NULL);
                    } else if (changed)
                        nt_ui_render();
                    bsp_lvgl_unlock();
                }
            }
            free(m->snapshot);
            m->snapshot = NULL;
        }
        if (!screen_off && nt_idle_should_blank(esp_timer_get_time(), last_input_us, nt_store_sleep_seconds())) {
            if (bsp_lvgl_lock(-1)) {
                nt_ui_sleep(true);
                ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_lvgl_sleep(true));
                ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_display_sleep(true));
                bsp_lvgl_unlock();
                screen_off = true;
                nt_power_busy(false);
                nt_network_display(false);
            }
        }
        if (!screen_off && bsp_lvgl_lock(-1)) {
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
