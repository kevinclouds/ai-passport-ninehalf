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
#include <sys/time.h>
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
    bool failed, history;
    int64_t deadline;
} reader_t;
static int read_chunk(reader_t *r) {
    while (!provision_requested && esp_timer_get_time() < r->deadline) {
        if (r->history && !nt_history_budget((time(NULL) + 8 * 3600) % 86400))
            return -1;
        int n = esp_http_client_read(r->client, (char *)r->buf, sizeof(r->buf));
        if (r->history && !nt_history_budget((time(NULL) + 8 * 3600) % 86400))
            return -1;
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
static esp_http_client_handle_t http_client;
static response_headers_t headers;
static void close_client(void) {
    if (http_client) {
        esp_http_client_close(http_client);
        esp_http_client_cleanup(http_client);
        http_client = NULL;
    }
}
static bool fetch(const char *url, nt_model_t *snapshot, nt_live_t *live) {
    unsigned history_budget = nt_history_budget((time(NULL) + 8 * 3600) % 86400);
    if (!live && !history_budget)
        return false;
    headers = (response_headers_t){0};
    esp_http_client_config_t cfg = {.event_handler = response_event,
                                    .user_data = &headers,
                                    .url = url,
                                    .method = HTTP_METHOD_GET,
                                    .timeout_ms = 15000,
                                    .crt_bundle_attach = esp_crt_bundle_attach,
                                    .buffer_size = 2048,
                                    .buffer_size_tx = 2048,
                                    .disable_auto_redirect = true};
    if (!http_client)
        http_client = esp_http_client_init(&cfg);
    esp_http_client_handle_t c = http_client;
    if (!c)
        return false;
    esp_http_client_set_url(c, url);
    esp_http_client_set_timeout_ms(c, !live && history_budget < 15 ? history_budget * 1000 : 15000);
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
    reader->history = live == NULL;
    if (provision_requested)
        goto done;
    esp_err_t err = esp_http_client_open(c, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTPS open failed: %s", esp_err_to_name(err));
        goto done;
    }
    history_budget = nt_history_budget((time(NULL) + 8 * 3600) % 86400);
    if (provision_requested || (!live && !history_budget))
        goto done;
    if (!live)
        esp_http_client_set_timeout_ms(c, history_budget < 15 ? history_budget * 1000 : 15000);
    if (esp_http_client_fetch_headers(c) < 0) {
        ESP_LOGE(TAG, "HTTPS headers failed");
        goto done;
    }
    int status = esp_http_client_get_status_code(c);
    ESP_LOGI(TAG, "HTTPS status=%d encoding=%s", status,
             headers.encoding[0] ? headers.encoding : "identity");
    history_budget = nt_history_budget((time(NULL) + 8 * 3600) % 86400);
    if (status != 200 || provision_requested || (!live && !history_budget))
        goto done;
    esp_http_client_set_timeout_ms(c, 1000);
    unsigned body_seconds = live ? 15 : history_budget;
    reader->deadline = esp_timer_get_time() + (int64_t)body_seconds * 1000000;
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
        if (live)
            *live = stream->live;
        ESP_LOGI(TAG, "Parsed records=%u", (unsigned)snapshot->count);
    } else {
        ok = false;
        ESP_LOGE(TAG, "Response body or JSON validation failed");
    }
done:
    free(stream);
    free(reader);
    time_t bj_now = time(NULL) + 8 * 3600;
    int seconds = bj_now % 86400;
    if (!ok || !esp_http_client_is_persistent_connection(c) || seconds < 21 * 3600 + 30 * 60 || seconds >= 21 * 3600 + 40 * 60 ||
        provision_requested) {
        close_client();
    }
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
    bool need_history = true, was_online = false, was_live = false;
    nt_model_t *snapshot = NULL;
    ESP_LOGI(TAG, "Network worker ready");
    for (;;) {
        uint32_t flags = 0;
        xTaskNotifyWait(0, UINT32_MAX, &flags, pdMS_TO_TICKS(due == 0 && was_live ? 1 : 1000));
        int64_t mono = esp_timer_get_time() / 1000000;
        if ((flags & REQUEST_PROVISION) || provision_requested) {
            ESP_LOGI(TAG, "Processing provisioning request");
            close_client();
            was_live = false;
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
            close_client();
            was_live = false;
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
        int seconds_now = (now + 8 * 3600) % 86400;
        bool live_window = seconds_now >= 21 * 3600 + 30 * 60 &&
                           seconds_now < 21 * 3600 + 40 * 60;
        if (live_window && !was_live) {
            due = 0;
            need_history = true;
        }
        if (!live_window && was_live)
            due = 0;
        was_live = live_window;
        if (mono < due)
            continue;
        bool show_loading = !live_window || (flags & REQUEST_REFRESH);
        if (show_loading)
            nt_app_sync_started();
        char url[120];
        struct timeval request_time;
        gettimeofday(&request_time, NULL);
        snprintf(url, sizeof(url), "https://macaumarksix.com/api/live2?time=%lld",
                 (long long)request_time.tv_sec * 1000 + request_time.tv_usec / 1000);
        snapshot = calloc(1, sizeof(*snapshot));
        nt_live_t live = {0};
        bool ok = snapshot && fetch(url, snapshot, &live);
        if (provision_requested) {
            free(snapshot);
            snapshot = NULL;
            nt_app_sync_finished(true);
            due = 0;
            continue;
        }
        if (ok) {
            if (snapshot->count)
                nt_app_network_result(snapshot, true);
            if (live.present &&
                (!snapshot->count || strcmp(live.record.issue, snapshot->records[0].issue) > 0))
                nt_app_live_result(&live);
            if (need_history && !provision_requested &&
                nt_history_budget((time(NULL) + 8 * 3600) % 86400)) {
                struct tm t;
                time_t beijing = now + 8 * 3600;
                gmtime_r(&beijing, &t);
                snprintf(url, sizeof(url), "https://history.macaumarksix.com/history/macaujc2/y/%d",
                         t.tm_year + 1900);
                if (fetch(url, snapshot, NULL)) {
                    nt_app_history_result(snapshot);
                    need_history = false;
                }
            }
        } else
            nt_app_network_result(NULL, false);
        time_t bj = time(NULL) + 8 * 3600;
        int32_t today = bj / 86400;
        bool today_done = ok && snapshot && snapshot->count && snapshot->records[0].day == today;
        free(snapshot);
        snapshot = NULL;
        if (provision_requested) {
            nt_app_sync_finished(true);
            due = 0;
            continue;
        }
        nt_app_sync_finished(ok);
        struct tm t;
        gmtime_r(&bj, &t);
        int seconds = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
        if (ok)
            failures = 0;
        else if (failures < 5)
            failures++;
        unsigned interval = nt_poll_interval(seconds, today_done, ok, failures);
        if (interval == UINT32_MAX || (!live_window && failures >= 3))
            interval = (seconds < 21 * 3600 + 30 * 60 ? 0 : 86400) - seconds +
                       21 * 3600 + 30 * 60;
        due = interval ? esp_timer_get_time() / 1000000 + interval : 0;
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
