#include "ninehalf_ui.h"
#include "ninehalf_network.h"
#include "ninehalf_store.h"
#include "ninehalf_wifi.h"
#include "bsp_display.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "lvgl.h"
#include "src/misc/lv_text_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
LV_FONT_DECLARE(nt_wifi_font_13);
LV_FONT_DECLARE(nt_font_8);
LV_FONT_DECLARE(nt_font_16);
LV_FONT_DECLARE(nt_font_9);
LV_FONT_DECLARE(nt_font_10);
LV_FONT_DECLARE(nt_font_11);
LV_FONT_DECLARE(nt_font_12);
LV_FONT_DECLARE(nt_font_13);
LV_FONT_DECLARE(nt_font_15);
LV_FONT_DECLARE(nt_font_17);
LV_FONT_DECLARE(nt_font_18);
LV_FONT_DECLARE(nt_font_19);
LV_FONT_DECLARE(nt_font_20);
LV_FONT_DECLARE(nt_font_24);
static nt_model_t *model;
static nt_live_t live;
static lv_obj_t *root, *scroll, *countdown, *time_label, *stat_rows[12], *record_rows[30];
static int page, focus = -1, observe_focus = -2, setting_focus = -1;
static int zodiac_focus, zodiac_offset;
typedef struct {
    const char *name, *branch, *polarity, *kind;
    uint8_t wave[5];
    const char *element[5];
} zodiac_row_t;
/* The user-approved 2026 reference card, in number order; not a date-based mapping. */
static const zodiac_row_t zodiac_2026[12] = {
    {"马", "午", "阴", "家禽", {0, 0, 1, 1, 2}, {"水", "金", "木", "土", "火"}},
    {"蛇", "巳", "阴", "野兽", {0, 1, 1, 2}, {"火", "水", "金", "木"}},
    {"龙", "辰", "阴", "野兽", {1, 1, 2, 2}, {"火", "水", "金", "木"}},
    {"兔", "卯", "阳", "野兽", {1, 2, 2, 0}, {"金", "木", "土", "火"}},
    {"虎", "寅", "阳", "野兽", {2, 2, 0, 1}, {"金", "木", "土", "火"}},
    {"牛", "丑", "阴", "家禽", {2, 0, 0, 1}, {"土", "火", "水", "金"}},
    {"鼠", "子", "阴", "野兽", {0, 0, 1, 2}, {"土", "火", "水", "金"}},
    {"猪", "亥", "阴", "家禽", {0, 1, 2, 2}, {"木", "土", "火", "水"}},
    {"狗", "戌", "阴", "家禽", {1, 2, 2, 0}, {"木", "土", "火", "水"}},
    {"鸡", "酉", "阴", "家禽", {1, 2, 0, 0}, {"火", "水", "金", "木"}},
    {"猴", "申", "阳", "野兽", {2, 0, 0, 1}, {"火", "水", "金", "木"}},
    {"羊", "未", "阳", "家禽", {0, 0, 1, 1}, {"金", "木", "土", "火"}},
};
static bool current_pending, offline_notice;
static int wifi_focus = -1, wifi_action;
static char wifi_selected[33];
static bool special = true, sort_miss = true, editing, sync_failed, syncing, foreground_loading;
static uint32_t notice_until;
static lv_obj_t *loading_area, *loading_text;
static bool live_active(void);
void nt_ui_sync_begin(void) {
    syncing = true;
    sync_failed = false;
    if (model && !model->count)
        foreground_loading = true;
}
void nt_ui_sync_state(bool success) {
    bool had_loading = loading_area != NULL;
    foreground_loading = false;
    syncing = false;
    sync_failed = !success;
    if (had_loading)
        nt_ui_render();
}
void nt_ui_notify(void) {
    offline_notice = false;
    notice_until = lv_tick_get() + 4000;
}
static uint32_t waves[] = {0xcf333c, 0x285cc6, 0x187b47};
static const lv_font_t *font(int size) {
    switch (size) {
    case 8:
        return &nt_font_9;
    case 16:
        return &nt_font_18;
    case 9:
        return &nt_font_10;
    case 10:
        return &nt_font_11;
    case 11:
        return &nt_font_12;
    case 12:
        return &nt_font_13;
    case 13:
        return &nt_font_15;
    case 15:
        return &nt_font_16;
    case 17:
        return &nt_font_18;
    case 18:
        return &nt_font_20;
    case 20:
        return &nt_font_20;
    default:
        return &nt_font_24;
    }
}
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, const char *text, int size,
                       uint32_t color) {
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_font(o, font(size), 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    return o;
}
static void right(lv_obj_t *p, int x, int y, int w, const char *t, int size, uint32_t c) {
    lv_obj_t *o = label(p, x, y, w, t, size, c);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_RIGHT, 0);
}
static void line(lv_obj_t *p, int y) {
    box(p, 0, y, 220, 1, 0xdedee5, 0);
}
static int number_padding(const lv_font_t *face, const char *text, int height) {
    int ascent = 0, descent = 0;
    for (uint32_t index = 0; text[index];) {
        uint32_t code = lv_text_encoded_next(text, &index);
        lv_font_glyph_dsc_t glyph;
        if (face->get_glyph_dsc(face, &glyph, code, 0)) {
            int top = glyph.box_h + glyph.ofs_y;
            if (top > ascent)
                ascent = top;
            if (-glyph.ofs_y > descent)
                descent = -glyph.ofs_y;
        }
    }
    int padding = (height - ascent - descent) / 2 - (face->line_height - face->base_line - ascent);
    return padding > 0 ? padding : 0;
}
static void tiles(lv_obj_t *parent, int y, const nt_record_t *r, bool current,
                  uint8_t number_mask, uint8_t wave_mask, uint8_t zodiac_mask) {
    lv_obj_update_layout(parent);
    int span = lv_obj_get_content_width(parent) - 41;
    for (int i = 0; i < 7; i++) {
        int x = 4 + i * span / 6 + (i == 6 ? 3 : 0);
        bool visible = r && (number_mask & (1U << i));
        bool colored = visible && (wave_mask & (1U << i));
        lv_obj_t *tile = label(parent, x, y, 27, visible ? r->number[i] : "",
                               current ? 18 : 16, colored ? 0xffffff : 0x39393f);
        lv_obj_set_size(tile, 27, current ? 30 : 27);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(tile,
                                  lv_color_hex(colored ? waves[r->wave[i] % 3] : 0xd9d9e1), 0);
        lv_obj_set_style_radius(tile, 4, 0);
        lv_obj_set_style_pad_top(
            tile, number_padding(font(current ? 18 : 16), visible ? r->number[i] : "",
                                 current ? 30 : 27),
            0);
        lv_obj_set_style_text_align(tile, LV_TEXT_ALIGN_CENTER, 0);
        if (i == 6 && visible) {
            lv_obj_set_style_outline_width(tile, 2, 0);
            lv_obj_set_style_outline_pad(tile, 1, 0);
            lv_obj_set_style_outline_color(tile, lv_color_hex(0x292c35), 0);
        }
        lv_obj_t *z =
            label(parent, x, y + (current ? 34 : 29), 27,
                  r && (zodiac_mask & (1U << i)) ? nt_zodiac_names[r->zodiac[i] % 12] : "",
                  current ? 11 : 10, 0x39393f);
        lv_obj_set_style_text_align(z, LV_TEXT_ALIGN_CENTER, 0);
    }
}
static void footer(const char *left, const char *text) {
    lv_obj_t *f = box(root, 0, 293, 240, 27, 0xeeeef3, 0);
    label(f, 12, 6, 128, left, 8, 0x45454d);
    right(f, 141, 6, 87, text, 8, 0x45454d);
}
static void countdown_text(const char *text, int size) {
    const lv_font_t *face = font(size);
    lv_obj_set_style_text_font(countdown, face, 0);
    lv_obj_set_height(countdown, 30);
    lv_obj_set_style_pad_top(countdown, number_padding(face, text, 30), 0);
    lv_label_set_text(countdown, text);
}
void nt_ui_clock(void) {
    if (page >= 3 && page <= 5) {
        static nt_wifi_view_t previous;
        static uint32_t checked;
        static bool valid;
        if (!valid || lv_tick_get() - checked >= 5000) {
            nt_wifi_view_t current;
            nt_wifi_view(&current);
            current.current_signal = !current.online                 ? 0
                                     : current.current_signal >= -55 ? 4
                                     : current.current_signal >= -67 ? 3
                                     : current.current_signal >= -80 ? 2
                                                                     : 1;
            checked = lv_tick_get();
            if (!valid || memcmp(&previous, &current, sizeof(current))) {
                previous = current;
                valid = true;
                nt_ui_render();
            }
        }
        return;
    }
    if (notice_until && (int32_t)(lv_tick_get() - notice_until) >= 0) {
        notice_until = 0;
        nt_ui_render();
        return;
    }
    if (page == 0 && !loading_area &&
        current_pending != (live_active() || nt_current_pending(model, time(NULL)))) {
        focus = -1;
        nt_ui_render();
        return;
    }
    if (!countdown || !time_label)
        return;
    time_t now = time(NULL);
    if (now < 1700000000) {
        countdown_text("时间未同步", 12);
        return;
    }
    int32_t today = (now + 8 * 3600) / 86400;
    bool received = model->count && model->records[0].day == today;
    int64_t delta = nt_countdown(now, received);
    lv_label_set_text(time_label, received ? "明日 21:32 揭晓" : "今日 21:32 揭晓");
    char text[32];
    if (delta == 0)
        strcpy(text, "正在摇号");
    else
        snprintf(text, sizeof(text), "%02lld:%02lld:%02lld", (long long)(delta / 3600),
                 (long long)(delta / 60 % 60), (long long)(delta % 60));
    countdown_text(text, delta == 0 ? 15 : 20);
}
static bool live_active(void) {
    if (!live.present || time(NULL) < 1700000000)
        return false;
    int32_t today = (time(NULL) + 8 * 3600) / 86400;
    return live.record.day == today &&
           (!model->count || strcmp(live.record.issue, model->records[0].issue) > 0);
}
static void home(void) {
    current_pending = live_active() || nt_current_pending(model, time(NULL));
    int offset = current_pending ? 0 : 1;
    int last = (int)model->count - offset - 1;
    if (focus > last)
        focus = last;
    scroll = box(root, 8, 32, 224, 261, 0xfafafc, 0);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(scroll, 240, 0);
    if (focus < 0)
        label(scroll, 0, 5, 140, "本期记录", 15, 0x1d1d1f);
    if (!model->count && !live_active()) {
        label(scroll, 0, 40, 210, "暂无记录，请联网同步", 12, 0x616166);
        footer("OK 刷新", "长按 UP 观察");
        return;
    }
    if (focus < 0) {
        const nt_record_t *r = live_active() ? &live.record
                               : current_pending ? NULL : &model->records[0];
        right(scroll, 138, 8, 82, r ? r->issue : "", 10, 0x565661);
        tiles(scroll, 30, r, true, live_active() ? (1U << live.revealed) - 1 : 0x7f,
              live_active() ? live.wave_mask : 0x7f,
              live_active() ? live.zodiac_mask : 0x7f);
        time_label = label(scroll, 0, 87, 120, "今日 21:32 揭晓", 10, 0x565661);
        countdown = label(scroll, 128, 81, 92, "", 20, 0x23232a);
        lv_obj_set_style_text_align(countdown, LV_TEXT_ALIGN_RIGHT, 0);
        line(scroll, 115);
        label(scroll, 0, 127, 130, "往期记录", 13, 0x1d1d1f);
        char count[32];
        snprintf(count, sizeof(count), "最近 %u 期", (unsigned)model->count);
        right(scroll, 148, 129, 72, count, 9, 0x565661);
    }
    size_t first = focus < 0 ? (size_t)offset : (size_t)focus + offset;
    for (size_t i = first; i < model->count && i < first + 5; i++) {
        bool selected = focus == (int)i - offset;
        lv_obj_t *row = box(scroll, 0, (focus < 0 ? 151 : 0) + (i - first) * 82, 224, 82,
                            selected ? 0xedf2fa : 0xfafafc, 2);
        record_rows[i - offset] = row;
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xfafafc), 0);
        if (selected) {
            lv_obj_set_style_border_color(row, lv_color_hex(0xc1cee3), 0);
        } else if (focus != (int)i - offset + 1)
            box(row, 0, 81, 222, 1, 0xdedee5, 0);
        label(row, 3, 8, 95, model->records[i].issue, 10, 0x565660);
        char date[16];
        snprintf(date, sizeof(date), "%.5s %.5s", model->records[i].timestamp + 5,
                 model->records[i].timestamp + 11);
        right(row, 108, 8, 111, date, 10, 0x565660);
        tiles(row, 31, &model->records[i], false, 0x7f, 0x7f, 0x7f);
    }
    lv_obj_update_layout(scroll);
    if (focus >= 0 && focus < (int)model->count - offset)
        lv_obj_scroll_to_y(scroll, lv_obj_get_y(record_rows[focus]), LV_ANIM_OFF);
    footer("到底再按 DOWN 回顶部", "长按 UP 观察");
    nt_ui_clock();
}
static void observe(void) {
    label(root, 8, 34, 150, "近30期观察", 18, 0x1d1d1f);
    char loaded[24];
    snprintf(loaded, sizeof(loaded), "已载入 %u期", (unsigned)model->count);
    right(root, 160, 39, 68, loaded, 9, 0x5b606d);
    lv_obj_t *tabs = box(root, 8, 65, 224, 32, observe_focus == -2 ? 0xdce6f5 : 0xe9eaf0, 6);
    lv_obj_set_style_border_width(tabs, 1, 0);
    lv_obj_set_style_border_color(tabs, lv_color_hex(0xe9eaf0), 0);
    if (observe_focus == -2) {
        lv_obj_set_style_border_color(tabs, lv_color_hex(0x8ba3c9), 0);
    }
    for (int i = 0; i < 2; i++) {
        lv_obj_t *tab =
            box(tabs, 3 + i * 110, 3, 106, 24,
                special == (i == 0) ? 0xffffff : (observe_focus == -2 ? 0xdce6f5 : 0xe9eaf0), 4);
        lv_obj_t *name = label(tab, 0, 4, 106, i ? "平特生肖" : "特码生肖", 12,
                               special == (i == 0) ? 0x243552 : 0x5b606d);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_t *sort = box(root, 8, 103, 224, 25, observe_focus == -1 ? 0xeaf0fa : 0xfafafc, 4);
    label(sort, 5, 5, 165, sort_miss ? "按连续未出现期数降序" : "按近7天出现期数升序", 10,
          0x253a60);
    right(sort, 163, 5, 48, "OK 切换", 9, 0x555c69);
    scroll = box(root, 8, 132, 224, 161, 0xfafafc, 0);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_bottom(scroll, 74, 0);
    nt_stat_t stats[12];
    time_t now = time(NULL);
    nt_stats(model, special, (now + 8 * 3600) / 86400, stats);
    int order[12];
    for (int i = 0; i < 12; i++)
        order[i] = i;
    for (int i = 0; i < 12; i++)
        for (int j = i + 1; j < 12; j++) {
            int a = sort_miss ? stats[order[i]].misses : stats[order[i]].week,
                b = sort_miss ? stats[order[j]].misses : stats[order[j]].week;
            if ((sort_miss ? b > a : b < a) || (a == b && order[j] < order[i])) {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    for (int i = observe_focus < 0 ? 0 : observe_focus;
         i < 12 && i < (observe_focus < 0 ? 0 : observe_focus) + 3; i++) {
        nt_stat_t st = stats[order[i]];
        lv_obj_t *row = box(scroll, 0, (i - (observe_focus < 0 ? 0 : observe_focus)) * 68, 220, 68,
                            observe_focus == i ? 0xedf2fa : 0xfafafc, 4);
        stat_rows[i] = row;
        if (observe_focus == i) {
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(0xbdcbe1), 0);
        }
        if (observe_focus != i)
            box(row, 0, 67, 220, 1, 0xdfe3eb, 0);
        label(row, 5, 8, 44, nt_zodiac_names[order[i]], 24, 0x29374d);
        char rate[16];
        snprintf(rate, sizeof(rate), "%u期", st.week);
        right(row, 125, 6, 81, rate, 20, 0x29374d);
        char caption[32];
        snprintf(caption, sizeof(caption), "近7天出现期数");
        right(row, 104, 30, 102, caption, 9, 0x606572);
        char v[50];
        snprintf(v, sizeof(v), "近%u期 %u次", (unsigned)model->count, st.count);
        label(row, 6, 49, 70, v, 9, 0x606572);
        snprintf(v, sizeof(v), "连续未出现 %s%u期", st.count ? "" : "≥", st.misses);
        label(row, 76, 49, 88, v, 9, 0x606572);
        snprintf(v, sizeof(v), "连出 %u期", st.streak);
        right(row, 164, 49, 52, v, 9, 0x606572);
    }
    lv_obj_update_layout(scroll);
    if (observe_focus >= 0)
        lv_obj_scroll_to_y(scroll, 0, LV_ANIM_OFF);
    footer("到底再按 DOWN 回顶部", "长按UP生肖表");
}
static lv_obj_t *zodiac_text(lv_obj_t *parent, int x, int y, int w, int h, const char *text,
                            const lv_font_t *face, uint32_t color) {
    lv_obj_t *o = label(parent, x, y, w, text, 10, color);
    lv_obj_set_style_text_font(o, face, 0);
    lv_obj_set_height(o, h);
    lv_obj_set_style_pad_top(o, number_padding(face, text, h), 0);
    return o;
}
static void zodiac_page(void) {
    static const uint32_t colors[] = {0xcc3441, 0x315fc2, 0x1f805a};
    zodiac_text(root, 9, 25, 150, 23, "生肖号码", &nt_font_19, 0x202127);
    lv_obj_t *year = box(root, 197, 30, 35, 18, 0xe8eefb, 5);
    lv_obj_t *text = zodiac_text(year, 0, 0, 35, 18, "2026", &nt_font_11, 0x4762a0);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    zodiac_text(root, 9, 50, 111, 13, "12 生肖 · 49 个号码", &nt_font_10, 0x707581);
    for (int i = 0; i < 3; i++)
        box(root, 118 + i * 7, 55, 5, 5, colors[i], 3);
    zodiac_text(root, 141, 50, 91, 13, "· 下方为五行", &nt_font_10, 0x707581);
    int top = zodiac_focus * 52;
    if (top < zodiac_offset)
        zodiac_offset = top;
    if (top + 52 > zodiac_offset + 226)
        zodiac_offset = top + 52 - 226;
    scroll = box(root, 8, 70, 224, 226, 0xf8f9fc, 0);
    int first = zodiac_offset / 52;
    for (int i = first; i < 12 && i < first + 5; i++) {
        const zodiac_row_t *data = &zodiac_2026[i];
        bool selected = i == zodiac_focus;
        lv_obj_t *row = box(scroll, 0, i * 52 - zodiac_offset, 224, 52,
                            selected ? 0xeaf1ff : 0xf8f9fc, selected ? 7 : 0);
        if (selected) {
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(0x8da9df), 0);
            /* Keep child coordinates identical with and without the selection border. */
            lv_obj_set_style_pad_all(row, -1, 0);
        } else
            box(row, 0, 51, 224, 1, 0xe4e7ee, 0);
        box(row, 59, 7, 1, 37, selected ? 0xcbd8ee : 0xdfe4ee, 0);
        zodiac_text(row, 5, 8, 13, 16, data->branch, &nt_font_11, 0x6b7690);
        zodiac_text(row, 5, 26, 13, 16, data->polarity, &nt_font_11, 0x6b7690);
        zodiac_text(row, 22, 7, 30, 24, data->name, &nt_font_19, 0x202127);
        zodiac_text(row, 22, 31, 30, 16, data->kind, &nt_font_11, 0x626d7e);
        int count = i ? 4 : 5;
        for (int j = 0; j < count; j++) {
            int x = 65 + j * 157 / count;
            int width = 65 + (j + 1) * 157 / count - 3 - x;
            unsigned value = i + 1 + j * 12;
            char number[] = {(char)('0' + value / 10), (char)('0' + value % 10), '\0'};
            lv_obj_t *tile = zodiac_text(row, x, 4, width, 28, number, &nt_font_17, 0xffffff);
            lv_obj_set_style_text_align(tile, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(tile, lv_color_hex(colors[data->wave[j]]), 0);
            lv_obj_set_style_radius(tile, 5, 0);
            text = zodiac_text(row, x, 33, width, 16, data->element[j], &nt_font_11, 0x4f5868);
            lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        }
    }
    lv_obj_t *bar = box(root, 0, 296, 240, 24, 0xeff1f6, 0);
    char position[24];
    snprintf(position, sizeof(position), "%02d / 12", zodiac_focus + 1);
    zodiac_text(bar, 9, 0, 45, 24, position, &nt_font_10, 0x626976);
    text = zodiac_text(bar, 59, 0, 172, 24, "↑↓ 浏览 · 长按 OK 返回", &nt_font_10, 0x626976);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_RIGHT, 0);
}
static void wifi_page(void) {
    nt_wifi_view_t view;
    nt_wifi_view(&view);
    label(root, 8, 36, 216, "Wi-Fi 管理", 18, 0x1d1d1f);
    lv_obj_t *name = label(root, 8, 68, 176, view.online ? view.current : "未连接", 12, 0x565661);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(name, &nt_wifi_font_13, 0);
    int bars = !view.online                 ? 0
               : view.current_signal >= -55 ? 4
               : view.current_signal >= -67 ? 3
               : view.current_signal >= -80 ? 2
                                            : 1;
    if (view.online)
        for (int i = 0; i < 4; i++)
            box(root, 194 + i * 8, 80 - (i + 1) * 4, 5, (i + 1) * 4, i < bars ? 0x18753d : 0xd6d9e1,
                1);
    if (page == 3) {
        if (wifi_focus >= (int)view.count)
            wifi_focus = view.count - 1;
        if (!view.count)
            label(root, 8, 118, 216, "暂无已保存网络", 12, 0x565661);
        for (int i = 0; i < view.count; i++) {
            lv_obj_t *row =
                box(root, 8, 101 + i * 36, 224, 35, wifi_focus == i ? 0xedf2fa : 0xfafafc, 3);
            name = label(row, 5, 9, 161, view.names[i], 11, 0x1d1d1f);
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_font(name, &nt_wifi_font_13, 0);
            char signal[16];
            if (view.online && !strcmp(view.names[i], view.current))
                strcpy(signal, "已连接");
            else if (view.signal[i] != INT8_MIN)
                snprintf(signal, sizeof(signal), "%d dBm", view.signal[i]);
            else
                strcpy(signal, "未发现");
            right(row, 164, 10, 54, signal, 9, 0x565661);
        }
        footer(view.pending ? "正在处理" : "↑↓ 选择", "长按返回");
    } else {
        name = label(root, 8, 111, 216, wifi_selected, 13, 0x1d1d1f);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(name, &nt_wifi_font_13, 0);
        if (page == 5)
            label(root, 8, 145, 216, "删除此网络？", 12, 0x565661);
        const char *options[2] = {page == 5 ? "取消" : "连接", page == 5 ? "确认删除" : "删除"};
        for (int i = 0; i < 2; i++) {
            lv_obj_t *row =
                box(root, 8, 175 + i * 40, 224, 36, wifi_action == i ? 0xedf2fa : 0xfafafc, 3);
            label(row, 8, 9, 208, options[i], 12, 0x1d1d1f);
        }
        footer("↑↓ 选择", "OK 确认");
    }
}
static void settings(void) {
    label(root, 8, 37, 216, "设置", 20, 0x1d1d1f);
    const char *names[] = {"Wi-Fi", "重新配网", "揭晓提醒", "音量", "亮度", "自动睡眠", "睡眠时间"};
    int offset = setting_focus > 4 ? setting_focus - 4 : 0;
    for (int i = offset; i < offset + 5; i++) {
        bool sel = setting_focus == i;
        lv_obj_t *row = box(root, 8, 70 + (i - offset) * 43, 224, 43, sel ? 0xe9edf6 : 0xfafafc, 3);
        if (sel) {
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(0xc1cee3), 0);
        }
        label(row, 8, 12, 102, names[i], 12, 0x1d1d1f);
        if (i == 0)
            right(row, 110, 12, 96, nt_wifi_online() ? "已连接" : "未连接", 11,
                  nt_wifi_online() ? 0x18753d : 0x656570);
        else if (i == 1)
            right(row, 110, 12, 96,
                  nt_provision_pending()  ? "正在启动"
                  : nt_provision_failed() ? "配网失败"
                  : nt_provision_active() ? "等待手机"
                                          : "进入",
                  11, 0x565661);
        else if (i == 2) {
            lv_obj_t *toggle =
                box(row, 178, 14, 28, 16, model->settings.reminders ? 0x18753d : 0x777780, 8);
            box(toggle, model->settings.reminders ? 14 : 2, 2, 12, 12, 0xffffff, 6);
        } else if (i == 5) {
            right(row, 110, 12, 96, nt_store_sleep_seconds() ? "开启" : "关闭", 11, 0x565661);
        } else if (i == 6) {
            char v[16];
            unsigned seconds = nt_store_sleep_delay();
            if (seconds < 60)
                snprintf(v, sizeof(v), "%u 秒", seconds);
            else
                snprintf(v, sizeof(v), "%u 分钟", seconds / 60);
            right(row, 110, 12, 96, v, 11, nt_store_sleep_seconds() ? 0x565661 : 0xa0a0a8);
        } else {
            int val = i == 3 ? model->settings.volume : model->settings.brightness;
            box(row, 110, 21, 58, 4, 0xd6d9e1, 2);
            box(row, 110, 21, 58 * val / 100, 4, 0x285cc6, 2);
            char v[12];
            snprintf(v, sizeof(v), "%d%%", val);
            right(row, 170, 13, 36, v, 11, editing && sel ? 0x285cc6 : 0x565661);
        }
        if (!sel && setting_focus != i + 1)
            box(row, 0, 42, 216, 1, 0xe5e5ea, 0);
    }
    footer(editing ? "↑↓ 调节" : "↑↓ 选择", editing ? "OK 保存" : "OK 确认");
}
static void loading_rotate(void *obj, int32_t angle) {
    lv_arc_set_rotation(obj, angle % 360);
}
static void loading(void) {
    lv_obj_t *area = box(root, 0, 32, 240, 261, 0xfafafc, 0);
    loading_area = area;
    lv_obj_t *spinner = lv_arc_create(area);
    lv_obj_remove_style(spinner, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(spinner, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(spinner, 0, 360);
    lv_arc_set_angles(spinner, 0, 90);
    lv_obj_set_size(spinner, 36, 36);
    lv_obj_set_pos(spinner, 102, 88);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, spinner);
    lv_anim_set_exec_cb(&anim, loading_rotate);
    lv_anim_set_values(&anim, 0, 360);
    lv_anim_set_duration(&anim, 1000);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&anim, 0);
    lv_anim_start(&anim);
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0xe5e5ea), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x285cc6), LV_PART_INDICATOR);
    lv_obj_t *text =
        label(area, 12, 140, 216, time(NULL) < 1700000000 ? "正在校时" : "正在同步", 13, 0x565661);
    loading_text = text;
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    footer("同步中", "长按 UP 观察");
}
void nt_ui_render(void) {
    bool busy = page == 0 && nt_wifi_online() &&
                ((syncing && foreground_loading) ||
                 (!live_active() && ((!sync_failed && !model->count) ||
                                     (!model->count && time(NULL) < 1700000000))));
    if (busy && loading_area) {
        lv_label_set_text(loading_text, time(NULL) < 1700000000 ? "正在校时" : "正在同步");
        return;
    }
    loading_area = NULL;
    loading_text = NULL;
    if (root)
        lv_obj_delete(root);
    memset(record_rows, 0, sizeof(record_rows));
    root = box(lv_screen_active(), 0, 0, 240, 320, page == 6 ? 0xf8f9fc : 0xfafafc, 0);
    countdown = NULL;
    time_label = NULL;
    nt_wifi_view_t wifi;
    nt_wifi_view(&wifi);
    const char *wifi_text = wifi.online ? wifi.current : "离线";
    lv_point_t wifi_size;
    lv_text_get_size(&wifi_size, wifi_text, &nt_wifi_font_13, 0, 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    int wifi_width = wifi_size.x < 130 ? wifi_size.x : 130;
    lv_obj_t *ssid = label(root, 8, 8, wifi_width, wifi_text, 13, 0x525258);
    lv_obj_set_style_text_font(ssid, &nt_wifi_font_13, 0);
    lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
    int bars = !wifi.online                 ? 0
               : wifi.current_signal >= -55 ? 4
               : wifi.current_signal >= -67 ? 3
               : wifi.current_signal >= -80 ? 2
                                            : 1;
    if (wifi.online)
        for (int i = 0; i < 4; i++)
            box(root, 12 + wifi_width + i * 6, 24 - (i + 1) * 3, 4, (i + 1) * 3,
                i < bars ? 0x18753d : 0xd6d9e1, 1);
    int soc = bsp_battery_soc();
    char battery[20];
    if (soc < 0)
        strcpy(battery, "电量 --");
    else
        snprintf(battery, sizeof(battery), "电量 %d%%", soc);
    right(root, 170, 12, 58, battery, 9, 0x525258);
    if (page == 0) {
        if (busy)
            loading();
        else
            home();
    } else if (page == 1)
        observe();
    else if (page == 2)
        settings();
    else if (page == 6)
        zodiac_page();
    else
        wifi_page();
    if (notice_until && page != 6) {
        lv_obj_t *notice = box(root, 8, 265, 216, 24, 0xedf2fa, 4);
        label(notice, 8, 5, 200, offline_notice ? "离线，无法刷新" : "新期记录已揭晓", 10,
              0x243552);
    }
}
void nt_ui_create(nt_model_t *m) {
    model = m;
    nt_ui_render();
}
void nt_ui_set_live(const nt_live_t *value) {
    live = value ? *value : (nt_live_t){0};
    nt_ui_render();
}
void nt_ui_key(bsp_btn_t key, bsp_btn_ev_t ev) {
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG)
        return;
    if (key == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        if (page == 0)
            return;
        if (editing) {
            editing = false;
            nt_store_save(model);
        } else if (page == 6) {
            page = 1;
        } else {
            page = page == 5 ? 4 : page == 4 ? 3 : page == 3 ? 2 : 0;
            observe_focus = -2;
        }
        nt_ui_render();
        return;
    }
    if (page == 1 && key == BSP_BTN_UP && ev == BSP_BTN_LONG) {
        page = 6;
        zodiac_focus = zodiac_offset = 0;
        nt_ui_render();
        return;
    }
    if (page == 0 && ev == BSP_BTN_LONG && (key == BSP_BTN_UP || key == BSP_BTN_DOWN)) {
        foreground_loading = false;
        page = key == BSP_BTN_UP ? 1 : 2;
        observe_focus = -2;
        setting_focus = -1;
        editing = false;
        nt_ui_render();
        return;
    }
    if (ev != BSP_BTN_CLICK)
        return;
    int d = key == BSP_BTN_UP ? -1 : 1;
    if (page == 0) {
        if (key == BSP_BTN_OK) {
            if (nt_wifi_online()) {
                foreground_loading = true;
                if (!syncing) {
                    nt_ui_sync_begin();
                    nt_network_refresh();
                }
            } else {
                offline_notice = true;
                notice_until = lv_tick_get() + 2500;
            }
        } else if (key == BSP_BTN_UP && focus < 0) {
            foreground_loading = false;
            page = 1;
            observe_focus = -2;
        } else if (!loading_area) {
            int last = (int)model->count - (nt_current_pending(model, time(NULL)) ? 1 : 2);
            focus = d > 0 && last >= 0 && focus == last
                        ? -1
                        : nt_focus_step(focus, d, -1, last >= 0 ? last : -1);
        }
    } else if (page == 1) {
        if (key == BSP_BTN_OK) {
            if (observe_focus == -2)
                special = !special;
            else if (observe_focus == -1)
                sort_miss = !sort_miss;
        } else if (key == BSP_BTN_UP && observe_focus == -2) {
            page = 6;
            zodiac_focus = zodiac_offset = 0;
        } else {
            observe_focus =
                d > 0 && observe_focus == 11 ? -2 : nt_focus_step(observe_focus, d, -2, 11);
        }
    } else if (page == 6) {
        if (key != BSP_BTN_OK)
            zodiac_focus = (zodiac_focus + d + 12) % 12;
    } else if (page >= 3 && page <= 5) {
        if (page == 3) {
            nt_wifi_view_t view;
            nt_wifi_view(&view);
            if (key == BSP_BTN_OK) {
                if (wifi_focus < 0 || wifi_focus >= view.count)
                    return;
                strcpy(wifi_selected, view.names[wifi_focus]);
                wifi_action = 0;
                page = 4;
            } else if (view.count)
                wifi_focus = wifi_focus < 0 ? (d > 0 ? 0 : view.count - 1)
                                            : (wifi_focus + d + view.count) % view.count;
        } else if (key != BSP_BTN_OK)
            wifi_action = 1 - wifi_action;
        else if (page == 4) {
            if (wifi_action) {
                page = 5;
                wifi_action = 0;
            } else {
                nt_wifi_request(wifi_selected, false);
                page = 3;
            }
        } else {
            if (wifi_action)
                nt_wifi_request(wifi_selected, true);
            page = 3;
            wifi_focus = -1;
        }
    } else {
        if (key == BSP_BTN_OK) {
            if (setting_focus < 0)
                return;
            if (setting_focus == 0) {
                page = 3;
                wifi_focus = -1;
            } else if (setting_focus == 1)
                nt_network_provision();
            else if (setting_focus == 2) {
                model->settings.reminders = !model->settings.reminders;
                nt_store_save(model);
            } else if (setting_focus == 5)
                nt_store_sleep_set(!nt_store_sleep_seconds(), nt_store_sleep_delay());
            else if (setting_focus == 6) {
                if (nt_store_sleep_seconds()) {
                    const unsigned delays[] = {30, 60, 120, 300, 600};
                    unsigned index = 0;
                    while (index < 4 && delays[index] != nt_store_sleep_delay())
                        index++;
                    nt_store_sleep_set(true, delays[(index + 1) % 5]);
                }
            } else {
                editing = !editing;
                if (!editing)
                    nt_store_save(model);
            }
        } else if (editing) {
            uint8_t *v = setting_focus == 3 ? &model->settings.volume : &model->settings.brightness;
            int next = *v - d * 10;
            int min = setting_focus == 3 ? 0 : 10;
            *v = next < min ? min : next > 100 ? 100 : next;
            bsp_audio_set_volume(model->settings.volume);
            bsp_display_backlight(model->settings.brightness);
        } else
            setting_focus = setting_focus < 0 ? (d > 0 ? 0 : 6) : (setting_focus + d + 7) % 7;
    }
    nt_ui_render();
}
