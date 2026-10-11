#include "lvgl.h"
#include "ninehalf_ui.h"
#include "ninehalf_stream.h"
#include "ninehalf_wifi.h"
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
void bsp_display_backlight(uint8_t v) {
}
void bsp_audio_set_volume(uint8_t v) {
}
int bsp_battery_soc(void) {
    return 86;
}
static bool provisioning_pending;
static bool online = true;
static bool provision_failed;
static int provision_calls;
static int refresh_calls;
static int wifi_requests;
static bool wifi_remove;
bool nt_provision_pending(void) {
    return provisioning_pending;
}
bool nt_provision_failed(void) {
    return provision_failed;
}
bool nt_wifi_online(void) {
    return online;
}
bool nt_provision_active(void) {
    return false;
}
void nt_network_refresh(void) {
    refresh_calls++;
}
void nt_network_provision(void) {
    provision_calls++;
    provisioning_pending = true;
}
void nt_store_save(const nt_model_t *m) {
}
void nt_wifi_view(nt_wifi_view_t *v) {
    *v = (nt_wifi_view_t){.count = 2,
                          .online = online,
                          .current = "Home",
                          .current_signal = -50,
                          .names = {"Home", "Office"},
                          .signal = {-50, -70}};
}
void nt_wifi_request(const char *ssid, bool remove) {
    assert(!strcmp(ssid, "Home"));
    wifi_requests++;
    wifi_remove = remove;
}
static time_t mock_now = 1791198674;
time_t time(time_t *out) {
    if (out)
        *out = mock_now;
    return mock_now;
}
static unsigned char pixels[240 * 320 * 3];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++) {
            unsigned short v;
            memcpy(&v, p, 2);
            p += 2;
            int o = (y * 240 + x) * 3;
            pixels[o] = (v >> 11) * 255 / 31;
            pixels[o + 1] = ((v >> 5) & 63) * 255 / 63;
            pixels[o + 2] = (v & 31) * 255 / 31;
        }
    lv_display_flush_ready(d);
}
static void check_labels(lv_obj_t *obj) {
    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_point_t size;
        lv_text_get_size(&size, lv_label_get_text(obj), lv_obj_get_style_text_font(obj, 0),
                         lv_obj_get_style_text_letter_space(obj, 0), 0, LV_COORD_MAX,
                         LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_content_width(obj) &&
            lv_label_get_long_mode(obj) != LV_LABEL_LONG_DOT) {
            fprintf(stderr, "Clipped label: %s (%d > %d)\n", lv_label_get_text(obj), (int)size.x,
                    (int)lv_obj_get_content_width(obj));
            abort();
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++)
        check_labels(lv_obj_get_child(obj, i));
}
static lv_obj_t *find_arc(lv_obj_t *obj) {
    if (lv_obj_check_type(obj, &lv_arc_class))
        return obj;
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++) {
        lv_obj_t *arc = find_arc(lv_obj_get_child(obj, i));
        if (arc)
            return arc;
    }
    return NULL;
}
static bool has_text(lv_obj_t *obj, const char *text) {
    if (lv_obj_check_type(obj, &lv_label_class) && !strcmp(lv_label_get_text(obj), text))
        return true;
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); i++)
        if (has_text(lv_obj_get_child(obj, i), text))
            return true;
    return false;
}
static int header_bars(void) {
    lv_obj_t *root = lv_obj_get_child(lv_screen_active(), 0);
    int count = 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, i);
        if (lv_obj_get_width(child) == 4 && lv_obj_get_y(child) <= 24)
            count++;
    }
    return count;
}
static void check_number_center(int x, int y, int width, int height) {
    int top = height, bottom = -1;
    for (int dy = 0; dy < height; dy++)
        for (int dx = 0; dx < width; dx++) {
            unsigned char *p = &pixels[((y + dy) * 240 + x + dx) * 3];
            if (p[0] == 255 && p[1] == 255 && p[2] == 255) {
                if (dy < top)
                    top = dy;
                if (dy > bottom)
                    bottom = dy;
            }
        }
    assert(bottom >= top);
    assert(abs(top - (height - 1 - bottom)) <= 2);
}
static void frame(const char *name) {
    lv_refr_now(NULL);
    check_labels(lv_screen_active());
    char path[200];
    snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n240 320\n255\n");
    fwrite(pixels, 1, sizeof(pixels), f);
    fclose(f);
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    printf("%s: used=%zu largest=%zu\n", name, mem.total_size - mem.free_size,
           mem.free_biggest_size);
}
LV_FONT_DECLARE(nt_wifi_font_13);
LV_FONT_DECLARE(nt_font_10);
int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    puts("init");
    lv_init();
    lv_display_t *d = lv_display_create(240, 320);
    _Alignas(LV_DRAW_BUF_ALIGN) static unsigned char buf[240 * 20 * 2];
    lv_display_set_buffers(d, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    nt_model_t m = {.count = 30, .settings = {60, 80, true}};
    for (int i = 0; i < 30; i++) {
        nt_record_t *r = &m.records[i];
        snprintf(r->issue, sizeof(r->issue), "2026%03d", 277 - i);
        strcpy(r->timestamp, "2026-10-04 21:32:32");
        r->day = nt_day(r->timestamp);
        for (int j = 0; j < 7; j++) {
            snprintf(r->number[j], 3, "%02d", j + 1);
            r->wave[j] = j % 3;
            r->zodiac[j] = (i + j) % 12;
        }
    }
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        assert(f);
        nt_stream_t *stream = calloc(1, sizeof(*stream));
        assert(stream);
        char chunk[137];
        size_t n;
        while ((n = fread(chunk, 1, sizeof(chunk), f)))
            nt_stream_feed(stream, chunk, n);
        fclose(f);
        assert(nt_stream_finish(stream));
        nt_merge_records(&m, stream->result.records, stream->result.count);
        memcpy(m.records, stream->result.records, sizeof(m.records));
        m.count = stream->result.count;
        free(stream);
    }
    lv_mem_monitor_t pre;
    lv_mem_monitor(&pre);
    printf("pool %zu free %zu\n", pre.total_size, pre.free_size);
    lv_font_glyph_dsc_t glyph;
    assert(nt_font_10.get_glyph_dsc(&nt_font_10, &glyph, 0x9F98, 0) == false);
    assert(nt_font_10.get_glyph_dsc(&nt_font_10, &glyph, 0x9F8D, 0));
    assert(nt_wifi_font_13.get_glyph_dsc(&nt_wifi_font_13, &glyph, 0x9F98, 0));
    assert(nt_wifi_font_13.get_glyph_dsc(&nt_wifi_font_13, &glyph, 0x79BB, 0));
    assert(nt_wifi_font_13.get_glyph_dsc(&nt_wifi_font_13, &glyph, 0x7EBF, 0));
    puts("create");
    nt_ui_create(&m);
    puts("created");
    assert(header_bars() == 4);
    nt_ui_key(BSP_BTN_UP, BSP_BTN_LONG);
    nt_ui_sync_begin();
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    assert(refresh_calls == 0);
    assert(!find_arc(lv_screen_active()));
    assert(has_text(lv_screen_active(), "本期记录"));
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    assert(!has_text(lv_screen_active(), "本期记录"));
    nt_ui_key(BSP_BTN_UP, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(refresh_calls == 0 && find_arc(lv_screen_active()));
    nt_ui_key(BSP_BTN_UP, BSP_BTN_LONG);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    assert(!find_arc(lv_screen_active()));
    nt_ui_sync_state(true);
    nt_ui_render();
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    assert(has_text(lv_screen_active(), "本期记录"));
    frame("home");
    time_t before_wait = mock_now;
    mock_now = ((before_wait + 8 * 3600) / 86400) * 86400 + 12 * 3600 + 30 * 60 - 1;
    nt_ui_clock();
    frame("before-cutoff");
    assert(pixels[(77 * 240 + 25) * 3] > 150 &&
           pixels[(77 * 240 + 25) * 3 + 1] < 100);
    mock_now++;
    nt_ui_clock();
    frame("cutoff");
    assert(pixels[(77 * 240 + 25) * 3] > 200 && pixels[(77 * 240 + 25) * 3 + 1] > 200);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    frame("cutoff-history");
    assert(has_text(lv_screen_active(), "2026277"));
    for (int i = 0; i < 29; i++)
        nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    assert(has_text(lv_screen_active(), "本期记录"));
    mock_now = before_wait + 3 * 3600;
    nt_ui_clock();
    frame("waiting");
    assert(has_text(lv_screen_active(), "等待揭晓"));
    mock_now = before_wait;
    nt_ui_clock();
    frame("home");
    check_number_center(12, 62, 27, 30);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(refresh_calls == 1);
    nt_ui_render();
    frame("loading");
    lv_obj_t *loading_root = lv_obj_get_child(lv_screen_active(), 0);
    lv_obj_t *arc = find_arc(loading_root);
    assert(arc);
    int previous = lv_arc_get_rotation(arc);
    for (int i = 0; i < 120; i++) {
        lv_tick_inc(20);
        lv_timer_handler();
        int angle = lv_arc_get_rotation(arc);
        assert((angle - previous + 360) % 360 <= 15);
        previous = angle;
        if (i == 25 || i == 75) {
            nt_ui_render();
            assert(lv_obj_get_child(lv_screen_active(), 0) == loading_root);
            assert(find_arc(loading_root) == arc);
        }
    }
    assert(lv_arc_get_rotation(arc) != 0);
    nt_ui_sync_state(false);
    nt_ui_render();
    frame("sync-failed");
    nt_ui_sync_state(true);
    nt_ui_render();
    frame("sync-restored");
    time_t saved_now = mock_now;
    mock_now = 0;
    nt_ui_clock();
    frame("unsynced");
    mock_now = saved_now;
    nt_ui_clock();
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    frame("history");
    nt_ui_key(BSP_BTN_UP, BSP_BTN_CLICK);
    for (int cycle = 0; cycle < 5; cycle++) {
        for (int i = 0; i < 29; i++)
            nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        frame("last");
        assert(has_text(lv_screen_active(), "到底再按 DOWN 回顶部"));
        nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        assert(has_text(lv_screen_active(), "本期记录"));
        assert(has_text(lv_screen_active(), "到底再按 DOWN 回顶部"));
        for (int i = 0; i < 29; i++)
            nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        for (int i = 0; i < 29; i++)
            nt_ui_key(BSP_BTN_UP, BSP_BTN_CLICK);
        nt_ui_key(BSP_BTN_UP, BSP_BTN_LONG);
        frame("observe");
        nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
        nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
        assert(has_text(lv_screen_active(), "按近7天出现期数升序"));
        assert(has_text(lv_screen_active(), "近7天出现期数"));
        nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
        nt_ui_key(BSP_BTN_UP, BSP_BTN_CLICK);
        for (int i = 0; i < 13; i++)
            nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        frame("observe-last");
        assert(has_text(lv_screen_active(), "到底再按 DOWN 回顶部"));
        nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
        assert(has_text(lv_screen_active(), "到底再按 DOWN 回顶部"));
        frame("observe-top");
        nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    }
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_LONG);
    frame("settings");
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(provision_calls == 0);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    frame("wifi-management");
    nt_ui_clock();
    lv_obj_t *wifi_root = lv_obj_get_child(lv_screen_active(), 0);
    lv_tick_inc(1000);
    nt_ui_clock();
    assert(lv_obj_get_child(lv_screen_active(), 0) == wifi_root);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    frame("wifi-actions");
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    frame("wifi-delete");
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(wifi_requests == 0);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(wifi_requests == 1 && !wifi_remove);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(wifi_requests == 2 && wifi_remove);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(provision_calls == 1);
    assert(has_text(lv_screen_active(), "正在启动"));
    provisioning_pending = false;
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    nt_ui_key(BSP_BTN_UP, BSP_BTN_CLICK);
    frame("observe-from-top");
    assert(has_text(lv_screen_active(), "近30期观察"));
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_LONG);
    frame("settings-from-long-down");
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(provision_calls == 1);
    nt_ui_key(BSP_BTN_UP, BSP_BTN_CLICK);
    frame("settings-up-first");
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_LONG);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    provision_failed = true;
    nt_ui_render();
    frame("provision-failed");
    provision_failed = false;
    nt_ui_render();
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    frame("volume");
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    nt_ui_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    frame("brightness");
    nt_ui_key(BSP_BTN_OK, BSP_BTN_LONG);
    online = false;
    nt_ui_render();
    assert(header_bars() == 0);
    frame("offline");
    nt_ui_key(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(has_text(lv_screen_active(), "离线，无法刷新"));
    assert(m.count == 30);
    frame("offline-refresh");
    puts("LVGL viewport stress: PASS");
}
