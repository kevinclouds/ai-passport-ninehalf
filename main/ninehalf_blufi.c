// BLUFI over NimBLE: receive Wi-Fi STA credentials from EspBlufi mobile apps.
#include "ninehalf_network.h"
#include "ninehalf_wifi.h"
#include "nvs.h"
#include "esp_timer.h"
#include "demo_blufi_security.h"
#include "demo_radio.h"

#include "esp_blufi.h"
#include "esp_blufi_api.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "lwip/ip4_addr.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "demo_blufi";
// ESP Config 微信小程序默认只显示以 "BLUFI" 开头的设备。
static const char *DEVICE_NAME = "BLUFI_FoloPassport";

#define BLUFI_AP_LIST_COUNT 16

typedef enum {
    BLUFI_DEMO_OFF = 0,
    BLUFI_DEMO_STARTING,
    BLUFI_DEMO_ADVERTISING,
    BLUFI_DEMO_BLE_CONNECTED,
    BLUFI_DEMO_WIFI_CONNECTING,
    BLUFI_DEMO_WIFI_CONNECTED,
    BLUFI_DEMO_FAILED,
} blufi_demo_state_t;

static esp_netif_t *s_sta_netif;
static esp_event_handler_instance_t s_wifi_handler;
static esp_event_handler_instance_t s_ip_handler;
static wifi_config_t s_sta_config;
static volatile blufi_demo_state_t s_state;
static volatile esp_err_t s_error;
static char s_ip[16];
static bool s_wifi_initialized;
static bool s_wifi_started;
static bool s_wifi_handler_registered;
static bool s_ip_handler_registered;
static bool s_host_initialized;
static bool s_host_running;
static bool s_gatt_initialized;
static bool s_btc_initialized;
static bool s_profile_initialized;
static bool s_ble_connected;
static bool s_wifi_connecting;
static bool s_wifi_got_ip;
static bool s_provision_completed;
static bool s_provision_connect_requested;
static bool s_reconnect_after_disconnect;
static SemaphoreHandle_t s_host_stopped;

static nt_wifi_profiles_t s_profiles;
static portMUX_TYPE s_profiles_lock = portMUX_INITIALIZER_UNLOCKED;
static char s_command_ssid[33];
static bool s_command_remove;
static int8_t s_signal[NT_WIFI_LIMIT] = {INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN};
static uint8_t s_tried;
static int64_t s_attempt_at, s_scan_at;
static unsigned s_scan_failures;
static wifi_ps_type_t s_power_save = WIFI_PS_NONE;
static esp_err_t save_profiles(void) {
    nt_wifi_profiles_t copy;
    portENTER_CRITICAL(&s_profiles_lock);
    copy = s_profiles;
    portEXIT_CRITICAL(&s_profiles_lock);
    nvs_handle_t h;
    esp_err_t err = nvs_open("nt-wifi", NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, "profiles", &copy, sizeof(copy));
        if (err == ESP_OK)
            err = nvs_commit(h);
        if (err != ESP_OK)
            ESP_LOGW(TAG, "Wi-Fi profile save failed: %s", esp_err_to_name(err));
        nvs_close(h);
    }
    return err;
}
static esp_err_t remember_connection(void) {
    char ssid[33] = {0}, password[65] = {0};
    memcpy(ssid, s_sta_config.sta.ssid, 32);
    memcpy(password, s_sta_config.sta.password, 64);
    portENTER_CRITICAL(&s_profiles_lock);
    nt_wifi_remember(&s_profiles, ssid, password);
    memset(s_signal, INT8_MIN, sizeof(s_signal));
    s_tried = 0;
    portEXIT_CRITICAL(&s_profiles_lock);
    return save_profiles();
}
void nt_wifi_view(nt_wifi_view_t *view) {
    memset(view, 0, sizeof(*view));
    portENTER_CRITICAL(&s_profiles_lock);
    view->pending = s_command_ssid[0] != 0;
    view->count = s_profiles.count;
    for (int i = 0; i < view->count; i++) {
        memcpy(view->names[i], s_profiles.items[i].ssid, 33);
        view->signal[i] = s_signal[i];
    }
    portEXIT_CRITICAL(&s_profiles_lock);
    wifi_ap_record_t ap;
    if (s_wifi_got_ip && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        view->online = true;
        memcpy(view->current, ap.ssid, 32);
        view->current_signal = ap.rssi;
    }
}
void nt_wifi_request(const char *ssid, bool remove) {
    portENTER_CRITICAL(&s_profiles_lock);
    strncpy(s_command_ssid, ssid, 32);
    s_command_ssid[32] = 0;
    s_command_remove = remove;
    portEXIT_CRITICAL(&s_profiles_lock);
}
static bool disconnect_station(void) {
    if (!s_wifi_connecting && !s_wifi_got_ip)
        return true;
    esp_wifi_disconnect();
    for (int i = 0; i < 40 && (s_wifi_connecting || s_wifi_got_ip); i++)
        vTaskDelay(pdMS_TO_TICKS(50));
    return !s_wifi_connecting && !s_wifi_got_ip;
}
static bool connect_profile(const nt_wifi_profile_t *profile) {
    if (!disconnect_station())
        return false;
    memset(&s_sta_config, 0, sizeof(s_sta_config));
    memcpy(s_sta_config.sta.ssid, profile->ssid, strlen(profile->ssid));
    memcpy(s_sta_config.sta.password, profile->password, strlen(profile->password));
    if (esp_wifi_set_config(WIFI_IF_STA, &s_sta_config) != ESP_OK)
        return false;
    s_wifi_connecting = true;
    s_attempt_at = esp_timer_get_time() / 1000000;
    if (esp_wifi_connect() != ESP_OK) {
        s_wifi_connecting = false;
        return false;
    }
    return true;
}
void nt_wifi_manage(void) {
    if (s_host_initialized)
        return;
    char ssid[33];
    bool remove;
    nt_wifi_profile_t chosen = {0};
    portENTER_CRITICAL(&s_profiles_lock);
    memcpy(ssid, s_command_ssid, 33);
    remove = s_command_remove;
    for (int i = 0; i < s_profiles.count; i++)
        if (!strcmp(s_profiles.items[i].ssid, ssid))
            chosen = s_profiles.items[i];
    portEXIT_CRITICAL(&s_profiles_lock);
    if (!ssid[0] || !chosen.ssid[0] || s_host_initialized)
        return;
    if (remove) {
        bool current = !strncmp((char *)s_sta_config.sta.ssid, ssid, 32);
        if (current && !disconnect_station())
            return;
        portENTER_CRITICAL(&s_profiles_lock);
        nt_wifi_forget(&s_profiles, ssid);
        memset(s_signal, INT8_MIN, sizeof(s_signal));
        s_tried = 0;
        portEXIT_CRITICAL(&s_profiles_lock);
        save_profiles();
        if (current) {
            memset(&s_sta_config, 0, sizeof(s_sta_config));
            esp_wifi_set_config(WIFI_IF_STA, &s_sta_config);
        }
        s_scan_at = 0;
    } else if (!connect_profile(&chosen))
        return;
    portENTER_CRITICAL(&s_profiles_lock);
    if (s_command_remove == remove && !strcmp(s_command_ssid, ssid))
        s_command_ssid[0] = 0;
    portEXIT_CRITICAL(&s_profiles_lock);
}

static void send_wifi_report(esp_blufi_sta_conn_state_t state) {
    wifi_mode_t mode = WIFI_MODE_STA;
    esp_wifi_get_mode(&mode);
    esp_blufi_extra_info_t info = {0};
    size_t ssid_len = strnlen((const char *)s_sta_config.sta.ssid, sizeof(s_sta_config.sta.ssid));
    if (ssid_len > 0) {
        info.sta_ssid = s_sta_config.sta.ssid;
        info.sta_ssid_len = ssid_len;
    }
    esp_blufi_send_wifi_conn_report(mode, state, 0, &info);
}

static void send_wifi_list(void) {
    uint16_t count = BLUFI_AP_LIST_COUNT;
    wifi_ap_record_t *records = calloc(count, sizeof(*records));
    esp_blufi_ap_record_t *list = calloc(count, sizeof(*list));
    if (!records || !list) {
        free(records);
        free(list);
        esp_wifi_clear_ap_list();
        ESP_LOGE(TAG, "Wi-Fi list allocation failed");
        esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        return;
    }
    esp_err_t err = esp_wifi_scan_get_ap_records(&count, records);
    if (err != ESP_OK) {
        free(records);
        free(list);
        ESP_LOGE(TAG, "Wi-Fi scan records failed: %s", esp_err_to_name(err));
        esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        return;
    }
    for (uint16_t i = 0; i < count; i++) {
        list[i].rssi = records[i].rssi;
        memcpy(list[i].ssid, records[i].ssid, sizeof(list[i].ssid));
    }
    free(records);
    ESP_LOGI(TAG, "Wi-Fi scan results=%u; BLE connected=%d; DMA heap=%u largest=%u", count,
             s_ble_connected,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (s_ble_connected) {
        err = esp_blufi_send_wifi_list(count, list);
        ESP_LOGI(TAG, "Wi-Fi list queued: %s", esp_err_to_name(err));
    }
    free(list);
}

static void request_wifi_connect(void) {
    bool was_connected = s_wifi_got_ip;
    s_wifi_connecting = true;
    s_wifi_got_ip = false;
    s_state = BLUFI_DEMO_WIFI_CONNECTING;
    if (was_connected) {
        s_reconnect_after_disconnect = true;
        if (esp_wifi_disconnect() == ESP_OK)
            return;
        s_reconnect_after_disconnect = false;
    }
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        s_error = err;
        s_wifi_connecting = false;
        s_state = BLUFI_DEMO_FAILED;
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)base;
    if (id == WIFI_EVENT_STA_START) {
        s_scan_at = 0;
        nt_network_wifi_changed();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%u", event->reason);
        s_wifi_got_ip = false;
        s_ip[0] = '\0';
        if (s_reconnect_after_disconnect) {
            s_reconnect_after_disconnect = false;
            esp_wifi_connect();
            return;
        }
        if (s_state == BLUFI_DEMO_WIFI_CONNECTING && s_ble_connected) {
            send_wifi_report(ESP_BLUFI_STA_CONN_FAIL);
        }
        s_wifi_connecting = false;
        s_scan_at = 0;
        s_state = s_ble_connected ? BLUFI_DEMO_BLE_CONNECTED : BLUFI_DEMO_ADVERTISING;
        nt_network_wifi_changed();
    } else if (id == WIFI_EVENT_SCAN_DONE) {
        if (s_host_initialized)
            send_wifi_list();
    }
}

static void ip_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)base;
    if (id != IP_EVENT_STA_GOT_IP)
        return;
    ip_event_got_ip_t *event = data;
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    s_wifi_connecting = false;
    s_wifi_got_ip = true;
    nt_network_wifi_changed();
    wifi_ap_record_t ap;
    bool matches = esp_wifi_sta_get_ap_info(&ap) == ESP_OK &&
                   !strncmp((char *)ap.ssid, (char *)s_sta_config.sta.ssid, 32);
    if (matches)
        remember_connection();
    if (s_host_initialized && s_provision_connect_requested && matches)
        s_provision_completed = true;
    s_state = BLUFI_DEMO_WIFI_CONNECTED;
    if (s_ble_connected)
        send_wifi_report(ESP_BLUFI_STA_CONN_SUCCESS);
}

static void blufi_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE reset: %d", reason);
    s_error = ESP_FAIL;
    s_state = BLUFI_DEMO_FAILED;
}

static void blufi_sync(void) {
    ESP_LOGI(TAG, "NimBLE synchronized; initializing BLUFI profile");
    int rc = esp_blufi_profile_init();
    if (rc == 0) {
        s_profile_initialized = true;
    } else {
        s_error = rc;
        ESP_LOGE(TAG, "BLUFI profile init failed: %d", rc);
        s_state = BLUFI_DEMO_FAILED;
        nt_app_network_result(NULL, false);
    }
}

static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    if (s_host_stopped)
        xSemaphoreGive(s_host_stopped);
    nimble_port_freertos_deinit();
}

static void blufi_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_LOGI(TAG, "Starting BLUFI advertising: %s", DEVICE_NAME);
        esp_blufi_adv_start_with_name(DEVICE_NAME);
        if (!ble_gap_adv_active()) {
            s_error = ESP_FAIL;
            s_state = BLUFI_DEMO_FAILED;
            ESP_LOGE(
                TAG, "BLUFI advertising failed; DMA heap=%u largest=%u",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
            nt_app_network_result(NULL, false);
            break;
        }
        ESP_LOGI(TAG, "BLUFI advertising active");
        if (s_wifi_got_ip) {
            s_state = BLUFI_DEMO_WIFI_CONNECTED;
        } else if (s_wifi_connecting) {
            s_state = BLUFI_DEMO_WIFI_CONNECTING;
        } else {
            s_state = BLUFI_DEMO_ADVERTISING;
        }
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        s_ble_connected = true;
        esp_blufi_adv_stop();
        if (demo_blufi_security_init() != 0) {
            s_error = ESP_ERR_NO_MEM;
            s_state = BLUFI_DEMO_FAILED;
        } else if (!s_wifi_got_ip) {
            s_state = BLUFI_DEMO_BLE_CONNECTED;
        }
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        s_ble_connected = false;
        demo_blufi_security_deinit();
        esp_blufi_adv_start_with_name(DEVICE_NAME);
        s_state = s_wifi_got_ip ? BLUFI_DEMO_WIFI_CONNECTED : BLUFI_DEMO_ADVERTISING;
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        esp_wifi_set_mode(WIFI_MODE_STA);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(s_sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        s_sta_config.sta.bssid_set = true;
        esp_wifi_set_config(WIFI_IF_STA, &s_sta_config);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        if (param->sta_ssid.ssid_len >= sizeof(s_sta_config.sta.ssid)) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            break;
        }
        memset(s_sta_config.sta.ssid, 0, sizeof(s_sta_config.sta.ssid));
        memset(s_sta_config.sta.password, 0, sizeof(s_sta_config.sta.password));
        memset(s_sta_config.sta.bssid, 0, sizeof(s_sta_config.sta.bssid));
        s_sta_config.sta.bssid_set = false;
        s_sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        memcpy(s_sta_config.sta.ssid, param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        esp_wifi_set_config(WIFI_IF_STA, &s_sta_config);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        if (param->sta_passwd.passwd_len >= sizeof(s_sta_config.sta.password)) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            break;
        }
        memset(s_sta_config.sta.password, 0, sizeof(s_sta_config.sta.password));
        memcpy(s_sta_config.sta.password, param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        esp_wifi_set_config(WIFI_IF_STA, &s_sta_config);
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        s_provision_connect_requested = true;
        request_wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
        if (s_wifi_got_ip) {
            send_wifi_report(ESP_BLUFI_STA_CONN_SUCCESS);
        } else if (s_wifi_connecting) {
            send_wifi_report(ESP_BLUFI_STA_CONNECTING);
        } else {
            send_wifi_report(ESP_BLUFI_STA_CONN_FAIL);
        }
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
        wifi_scan_config_t config = {0};
        esp_err_t err = esp_wifi_scan_start(&config, false);
        ESP_LOGI(TAG, "Wi-Fi scan request: %s", esp_err_to_name(err));
        if (err != ESP_OK) {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        esp_blufi_send_error_info(param->report_error.state);
        break;
    default:
        break;
    }
}

static esp_blufi_callbacks_t s_callbacks = {
    .event_cb = blufi_event,
    .negotiate_data_handler = demo_blufi_negotiate,
    .encrypt_func = demo_blufi_encrypt,
    .decrypt_func = demo_blufi_decrypt,
    .checksum_func = demo_blufi_checksum,
};

esp_err_t nt_wifi_start(void) {
    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK)
        return err;
    err = demo_radio_network_prepare();
    if (err != ESP_OK)
        return err;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif)
        return ESP_ERR_NO_MEM;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&config);
    if (err != ESP_OK)
        return err;
    s_wifi_initialized = true;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL,
                                              &s_wifi_handler);
    if (err != ESP_OK)
        return err;
    s_wifi_handler_registered = true;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event, NULL,
                                              &s_ip_handler);
    if (err != ESP_OK)
        return err;
    s_ip_handler_registered = true;
    nvs_handle_t h;
    size_t size = sizeof(s_profiles);
    bool loaded = false;
    if (nvs_open("nt-wifi", NVS_READONLY, &h) == ESP_OK) {
        loaded = nvs_get_blob(h, "profiles", &s_profiles, &size) == ESP_OK &&
                 size == sizeof(s_profiles) && s_profiles.count <= NT_WIFI_LIMIT;
        nvs_close(h);
    }
    if (loaded) {
        for (int i = 0; i < s_profiles.count; i++)
            if (!memchr(s_profiles.items[i].ssid, 0, 33) ||
                !memchr(s_profiles.items[i].password, 0, 65))
                loaded = false;
    }
    if (!loaded) {
        memset(&s_profiles, 0, sizeof(s_profiles));
        esp_wifi_get_config(WIFI_IF_STA, &s_sta_config);
        err = remember_connection();
        if (err != ESP_OK)
            return err;
    }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t legacy = {0};
    esp_wifi_get_config(WIFI_IF_STA, &legacy);
    if (err == ESP_OK && legacy.sta.ssid[0]) {
        memset(&legacy, 0, sizeof(legacy));
        err = esp_wifi_set_config(WIFI_IF_STA, &legacy);
    }
    if (err == ESP_OK)
        err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK)
        err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK)
        err = esp_wifi_start();
    if (err == ESP_OK) {
        s_wifi_started = true;
        err = esp_wifi_set_inactive_time(WIFI_IF_STA, 4);
    }
    return err;
}

static esp_err_t provision_error(const char *stage, esp_err_t err) {
    s_error = err;
    s_state = BLUFI_DEMO_FAILED;
    ESP_LOGE(TAG, "Provisioning %s failed: %s", stage, esp_err_to_name(err));
    return err;
}

esp_err_t nt_provision_start(void) {
    if (s_host_initialized)
        return ESP_OK;
    s_error = ESP_OK;
    s_provision_completed = false;
    s_provision_connect_requested = false;
    ESP_LOGI(TAG, "Provisioning requested; heap=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    esp_err_t err = esp_blufi_register_callbacks(&s_callbacks);
    if (err != ESP_OK)
        return provision_error("callbacks", err);
    ESP_LOGI(TAG, "BLUFI callbacks registered; initializing NimBLE");
    err = nimble_port_init();
    if (err != ESP_OK)
        return provision_error("NimBLE", err);
    s_host_initialized = true;
    s_host_stopped = xSemaphoreCreateBinary();
    if (!s_host_stopped) {
        nimble_port_deinit();
        s_host_initialized = false;
        return provision_error("stop semaphore", ESP_ERR_NO_MEM);
    }
    ble_hs_cfg.reset_cb = blufi_reset;
    ble_hs_cfg.sync_cb = blufi_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    int rc = esp_blufi_gatt_svr_init();
    if (rc != 0) {
        nt_provision_stop();
        return provision_error("GATT services", ESP_FAIL);
    }
    s_gatt_initialized = true;
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        nt_provision_stop();
        return provision_error("device name", ESP_FAIL);
    }
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "GATT ready; heap=%u largest=%u", (unsigned)free_heap, (unsigned)largest);
    // BTC and NimBLE host stacks need 4608 + 4096 bytes, plus queues/security.
    if (free_heap < 16384 || largest < 4608) {
        nt_provision_stop();
        return provision_error("BLUFI task memory", ESP_ERR_NO_MEM);
    }
    ESP_LOGI(TAG, "Initializing BLUFI task");
    esp_blufi_btc_init();
    s_btc_initialized = true;
    err = esp_nimble_enable(host_task);
    if (err == ESP_OK) {
        s_host_running = true;
        ESP_LOGI(TAG, "NimBLE host started; awaiting synchronization");
    } else {
        nt_provision_stop();
        return provision_error("host task", err);
    }
    return ESP_OK;
}

bool nt_wifi_online(void) {
    return s_wifi_got_ip;
}
bool nt_provision_active(void) {
    return s_host_initialized;
}
void nt_wifi_expedite(void) {
    s_scan_failures = 0;
    s_scan_at = 0;
}
unsigned nt_wifi_retry_wait(void) {
    int64_t remaining = s_scan_at - esp_timer_get_time() / 1000000;
    if (s_host_initialized || s_wifi_connecting || remaining <= 0)
        return 1;
    return remaining > 60 ? 60 : (unsigned)remaining;
}
void nt_wifi_power_save(bool idle) {
    if (s_wifi_got_ip)
        s_scan_failures = 0;
    wifi_ps_type_t mode = idle && !s_host_initialized ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM;
    if (mode != s_power_save) {
        esp_err_t err = esp_wifi_set_ps(mode);
        if (err == ESP_OK)
            s_power_save = mode;
        else
            ESP_LOGW(TAG, "Wi-Fi power save: %s", esp_err_to_name(err));
    }
}
void nt_wifi_retry(void) {
    if (s_wifi_got_ip || s_host_initialized)
        return;
    int64_t now = esp_timer_get_time() / 1000000;
    if (s_wifi_connecting) {
        if (now - s_attempt_at < 20)
            return;
        if (!disconnect_station())
            return;
    }
    if (now < s_scan_at)
        return;
    nt_wifi_profiles_t profiles;
    portENTER_CRITICAL(&s_profiles_lock);
    profiles = s_profiles;
    portEXIT_CRITICAL(&s_profiles_lock);
    if (!profiles.count) {
        s_scan_at = now + 30;
        return;
    }
    wifi_scan_config_t scan = {0};
    if (esp_wifi_scan_start(&scan, true) != ESP_OK) {
        if (s_scan_failures < 7) s_scan_failures++;
        s_scan_at = now + nt_wifi_scan_interval(s_scan_failures);
        return;
    }
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    wifi_ap_record_t *aps = count ? calloc(count, sizeof(*aps)) : NULL;
    int8_t signal[NT_WIFI_LIMIT];
    memset(signal, INT8_MIN, sizeof(signal));
    if (aps && esp_wifi_scan_get_ap_records(&count, aps) == ESP_OK) {
        for (int i = 0; i < profiles.count; i++)
            for (int j = 0; j < count; j++)
                if (!strncmp(profiles.items[i].ssid, (char *)aps[j].ssid, 32) &&
                    aps[j].rssi > signal[i])
                    signal[i] = aps[j].rssi;
    } else
        esp_wifi_clear_ap_list();
    free(aps);
    portENTER_CRITICAL(&s_profiles_lock);
    memcpy(s_signal, signal, sizeof(signal));
    int best = nt_wifi_best(&profiles, signal, s_tried);
    if (best >= 0)
        s_tried |= 1U << best;
    else
        s_tried = 0;
    portEXIT_CRITICAL(&s_profiles_lock);
    if (s_scan_failures < 7) s_scan_failures++;
    s_scan_at = now + (best < 0 ? nt_wifi_scan_interval(s_scan_failures) : 5);
    if (best >= 0)
        connect_profile(&profiles.items[best]);
}
void nt_provision_stop(void) {
    if (!s_host_initialized)
        return;
    if (s_profile_initialized)
        esp_blufi_adv_stop();
    if (s_ble_connected)
        esp_blufi_disconnect();
    if (s_host_running) {
        if (nimble_port_stop() != 0)
            return;
        xSemaphoreTake(s_host_stopped, portMAX_DELAY);
    }
    if (s_gatt_initialized) {
        esp_blufi_gatt_svr_deinit();
    }
    nimble_port_deinit();
    if (s_profile_initialized)
        esp_blufi_profile_deinit();
    if (s_btc_initialized) {
        esp_blufi_btc_deinit();
    }
    demo_blufi_security_deinit();
    vSemaphoreDelete(s_host_stopped);
    s_host_stopped = NULL;
    s_host_initialized = false;
    s_host_running = false;
    s_profile_initialized = false;
    s_gatt_initialized = false;
    s_btc_initialized = false;
    s_ble_connected = false;
}

bool nt_provision_complete(void) {
    return s_provision_completed;
}

bool nt_provision_failed(void) {
    return s_error != ESP_OK;
}
