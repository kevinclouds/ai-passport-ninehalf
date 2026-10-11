#include "ninehalf_network.h"
#include "ninehalf_wifi.h"
#include "ninehalf_stream.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "miniz.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
static TaskHandle_t worker;
static volatile bool provision_requested;
#define REQUEST_REFRESH 1
#define REQUEST_PROVISION 2
#define WIFI_CHANGED 4
static const char *TAG = "nt_network";
typedef struct {
    esp_http_client_handle_t client;
    uint8_t buf[1024];
    size_t pos, size;
    bool failed;
    int64_t deadline;
} reader_t;
static int read_chunk(reader_t *r) {
    while (!provision_requested && esp_timer_get_time() < r->deadline) {
        int n = esp_http_client_read(r->client, (char *)r->buf, sizeof(r->buf));
        if (n != -ESP_ERR_HTTP_EAGAIN)
            return n;
    }
    return -1;
}
static int next_byte(reader_t *r) {
    if (r->pos == r->size) {
        int n = read_chunk(r);
        if (n <= 0)
            return -1;
        r->size = n;
        r->pos = 0;
    }
    return r->buf[r->pos++];
}
static bool gzip_body(reader_t *r, nt_stream_t *stream) {
    uint8_t head[10];
    for (int i = 0; i < 10; i++) {
        int b = next_byte(r);
        if (b < 0)
            return false;
        head[i] = b;
    }
    if (head[0] != 31 || head[1] != 139 || head[2] != 8 || (head[3] & 0xe0))
        return false;
    if (head[3] & 4) {
        int lo = next_byte(r), hi = next_byte(r);
        if (lo < 0 || hi < 0)
            return false;
        unsigned n = lo | (hi << 8);
        while (n--)
            if (next_byte(r) < 0)
                return false;
    }
    for (int flag = 8; flag <= 16; flag *= 2)
        if (head[3] & flag) {
            int b;
            unsigned n = 0;
            do {
                b = next_byte(r);
                if (b < 0 || ++n > 4096)
                    return false;
            } while (b);
        }
    if (head[3] & 2) {
        if (next_byte(r) < 0 || next_byte(r) < 0)
            return false;
    }
    tinfl_decompressor *d = calloc(1, sizeof(*d));
    uint8_t *out = malloc(32768);
    if (!d || !out) {
        ESP_LOGE(TAG, "gzip buffers unavailable");
        free(d);
        free(out);
        return false;
    }
    tinfl_init(d);
    size_t offset = 0;
    uint32_t crc = 0, total = 0;
    bool ok = false;
    for (;;) {
        if (r->pos == r->size) {
            int n = read_chunk(r);
            if (n <= 0)
                break;
            r->size = n;
            r->pos = 0;
        }
        size_t in = r->size - r->pos, produced = 32768 - offset;
        tinfl_status status = tinfl_decompress(d, r->buf + r->pos, &in, out, out + offset,
                                               &produced, TINFL_FLAG_HAS_MORE_INPUT);
        r->pos += in;
        if (produced) {
            crc = mz_crc32(crc, out + offset, produced);
            total += produced;
            nt_stream_feed(stream, (char *)out + offset, produced);
            offset = (offset + produced) % 32768;
        }
        if (stream->failed || status < 0)
            break;
        if (status == TINFL_STATUS_DONE) {
            uint32_t expected_crc = 0, expected_size = 0;
            bool trailer = true;
            for (int i = 0; i < 8; i++) {
                int b = next_byte(r);
                if (b < 0) {
                    trailer = false;
                    break;
                }
                if (i < 4)
                    expected_crc |= (uint32_t)b << (8 * i);
                else
                    expected_size |= (uint32_t)b << (8 * (i - 4));
            }
            ok = trailer && crc == expected_crc && total == expected_size;
            break;
        }
        if (!in && !produced)
            break;
    }
    free(d);
    free(out);
    return ok;
}
typedef struct {
    char encoding[24];
} response_headers_t;
static esp_err_t response_event(esp_http_client_event_t *event) {
    response_headers_t *headers = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_HEADER &&
        !strcasecmp(event->header_key, "Content-Encoding")) {
        snprintf(headers->encoding, sizeof(headers->encoding), "%s", event->header_value);
    }
    return ESP_OK;
}
static bool fetch(const char *url, nt_model_t *snapshot) {
    response_headers_t headers = {0};
    esp_http_client_config_t cfg = {.event_handler = response_event,
                                    .user_data = &headers,
                                    .url = url,
                                    .method = HTTP_METHOD_GET,
                                    .timeout_ms = 15000,
                                    .crt_bundle_attach = esp_crt_bundle_attach,
                                    .buffer_size = 2048,
                                    .buffer_size_tx = 2048,
                                    .disable_auto_redirect = true};
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c)
        return false;
    esp_http_client_set_header(c, "Accept", "application/json, text/plain, */*");
    esp_http_client_set_header(c, "Accept-Encoding", "identity");
    esp_http_client_set_header(c, "Accept-Language", "zh-CN,zh;q=0.9");
    esp_http_client_set_header(c, "Sec-Fetch-Dest", "empty");
    esp_http_client_set_header(c, "Sec-Fetch-Mode", "cors");
    esp_http_client_set_header(c, "Sec-Fetch-Site", "cross-site");
    esp_http_client_set_header(c, "Sec-Ch-Ua-Mobile", "?0");
    esp_http_client_set_header(c, "Origin", "https://macaujc.com");
    esp_http_client_set_header(c, "Referer", "https://macaujc.com/");
    esp_http_client_set_header(c, "User-Agent",
                               "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
                               "(KHTML, like Gecko) Chrome/154.0.0.0 Safari/537.36");
    nt_stream_t *stream = calloc(1, sizeof(*stream));
    reader_t *reader = calloc(1, sizeof(*reader));
    bool ok = false;
    if (!stream || !reader)
        goto done;
    reader->client = c;
    if (provision_requested)
        goto done;
    esp_err_t err = esp_http_client_open(c, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTPS open failed: %s", esp_err_to_name(err));
        goto done;
    }
    if (provision_requested)
        goto done;
    if (esp_http_client_fetch_headers(c) < 0) {
        ESP_LOGE(TAG, "HTTPS headers failed");
        goto done;
    }
    int status = esp_http_client_get_status_code(c);
    ESP_LOGI(TAG, "HTTPS status=%d encoding=%s", status,
             headers.encoding[0] ? headers.encoding : "identity");
    if (status != 200 || provision_requested)
        goto done;
    esp_http_client_set_timeout_ms(c, 1000);
    reader->deadline = esp_timer_get_time() + 120000000;
    const char *encoding = headers.encoding;
    if (!strcasecmp(encoding, "gzip"))
        ok = gzip_body(reader, stream);
    else if (!encoding[0] || !strcasecmp(encoding, "identity")) {
        int n;
        while ((n = read_chunk(reader)) > 0) {
            nt_stream_feed(stream, (char *)reader->buf, n);
            if (stream->failed)
                break;
        }
        ok = n == 0 && esp_http_client_is_complete_data_received(c);
    }
    if (provision_requested)
        goto done;
    if (ok && nt_stream_finish(stream)) {
        *snapshot = stream->result;
        ESP_LOGI(TAG, "Parsed records=%u", (unsigned)snapshot->count);
    } else {
        ok = false;
        ESP_LOGE(TAG, "Response body or JSON validation failed");
    }
done:
    free(stream);
    free(reader);
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}
static void run(void *arg) {
    (void)arg;
    if (nt_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed");
        worker = NULL;
        nt_app_network_result(NULL, false);
        vTaskDelete(NULL);
        return;
    }
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp);
    int64_t due = 0, provision_until = 0;
    unsigned failures = 0;
    bool need_history = true, was_online = false;
    nt_model_t *snapshot = NULL;
    ESP_LOGI(TAG, "Network worker ready");
    for (;;) {
        uint32_t flags = 0;
        xTaskNotifyWait(0, UINT32_MAX, &flags, pdMS_TO_TICKS(1000));
        int64_t mono = esp_timer_get_time() / 1000000;
        if ((flags & REQUEST_PROVISION) || provision_requested) {
            ESP_LOGI(TAG, "Processing provisioning request");
            esp_err_t err = nt_provision_start();
            provision_requested = false;
            if (err == ESP_OK)
                provision_until = mono + 180;
            else
                ESP_LOGE(TAG, "Provisioning start failed: %s", esp_err_to_name(err));
            nt_app_network_result(NULL, err == ESP_OK);
        }
        if (nt_provision_active()) {
            if (mono >= provision_until || nt_provision_complete()) {
                nt_provision_stop();
                provision_until = 0;
            } else
                continue;
        }
        nt_wifi_manage();
        if (!nt_wifi_online()) {
            was_online = false;
            nt_wifi_retry();
            continue;
        }
        if (!was_online) {
            was_online = true;
            due = 0;
            need_history = true;
        }
        time_t now = time(NULL);
        if (now < 1700000000)
            continue;
        if (flags & REQUEST_REFRESH)
            due = 0;
        if (mono < due)
            continue;
        nt_app_sync_started();
        char url[120];
        snprintf(url, sizeof(url), "https://macaumarksix.com/api/live2?time=%lld",
                 (long long)now * 1000);
        snapshot = calloc(1, sizeof(*snapshot));
        bool ok = snapshot && fetch(url, snapshot);
        if (provision_requested) {
            free(snapshot);
            snapshot = NULL;
            nt_app_sync_finished(true);
            due = 0;
            continue;
        }
        if (ok) {
            nt_app_network_result(snapshot, true);
            if (need_history && !provision_requested) {
                struct tm t;
                time_t beijing = now + 8 * 3600;
                gmtime_r(&beijing, &t);
                snprintf(url, sizeof(url), "https://history.macaumarksix.com/history/macaujc2/y/%d",
                         t.tm_year + 1900);
                if (fetch(url, snapshot)) {
                    nt_app_history_result(snapshot);
                    need_history = false;
                }
            }
        } else
            nt_app_network_result(NULL, false);
        free(snapshot);
        snapshot = NULL;
        if (provision_requested) {
            nt_app_sync_finished(true);
            due = 0;
            continue;
        }
        nt_app_sync_finished(ok);
        struct tm t;
        time_t bj = now + 8 * 3600;
        gmtime_r(&bj, &t);
        unsigned interval = t.tm_hour == 21 && t.tm_min >= 30 && t.tm_min < 45 ? 15 : 300;
        if (ok)
            failures = 0;
        else {
            if (failures < 5)
                failures++;
            interval = 30U << failures;
        }
        due = mono + interval;
        ESP_LOGI(TAG, "sync %s; free heap %lu", ok ? "ok" : "failed",
                 (unsigned long)esp_get_free_heap_size());
    }
}
void nt_network_wifi_changed(void) {
    if (worker)
        xTaskNotify(worker, WIFI_CHANGED, eSetBits);
}
void nt_network_start(void) {
    if (xTaskCreate(run, "nt_network", 8192, NULL, 4, &worker) != pdPASS) {
        worker = NULL;
        ESP_LOGE(TAG, "Network task allocation failed");
        nt_app_network_result(NULL, false);
    }
}
void nt_network_refresh(void) {
    if (worker)
        xTaskNotify(worker, REQUEST_REFRESH, eSetBits);
}
void nt_network_provision(void) {
    ESP_LOGI(TAG, "UI requested provisioning; worker %s", worker ? "ready" : "unavailable");
    if (worker) {
        provision_requested = true;
        xTaskNotify(worker, REQUEST_PROVISION, eSetBits);
    } else
        nt_app_network_result(NULL, false);
}

bool nt_provision_pending(void) {
    return provision_requested;
}
