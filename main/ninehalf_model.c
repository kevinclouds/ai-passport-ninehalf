#include "ninehalf_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const char *const nt_zodiac_names[12] = {"鼠", "牛", "虎", "兔", "龍", "蛇",
                                         "馬", "羊", "猴", "雞", "狗", "豬"};
int nt_zodiac(const char *name) {
    for (int i = 0; i < 12; i++)
        if (!strcmp(name, nt_zodiac_names[i]))
            return i;
    const char *sc[] = {"龙", "马", "鸡", "猪"};
    const int ids[] = {4, 6, 9, 11};
    for (int i = 0; i < 4; i++)
        if (!strcmp(name, sc[i]))
            return ids[i];
    return -1;
}
int32_t nt_day(const char *s) {
    int y, m, d;
    if (sscanf(s, "%d-%d-%d", &y, &m, &d) != 3 || y < 2020 || m < 1 || m > 12 || d < 1 || d > 31)
        return -1;
    const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    if (d > limit)
        return -1;
    y -= m <= 2;
    int era = y / 400;
    unsigned yo = y - era * 400;
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + (int)(yo * 365 + yo / 4 - yo / 100 + doy) - 719468;
}
static int compare(const void *a, const void *b) {
    return strcmp(((const nt_record_t *)b)->timestamp, ((const nt_record_t *)a)->timestamp);
}
void nt_merge_records(nt_model_t *m, const nt_record_t *r, size_t n) {
    for (size_t j = 0; j < n; j++) {
        size_t i = 0;
        while (i < m->count && strcmp(m->records[i].issue, r[j].issue))
            i++;
        if (i < m->count)
            m->records[i] = r[j];
        else if (m->count < NT_RECORDS)
            m->records[m->count++] = r[j];
        else if (strcmp(r[j].timestamp, m->records[m->count - 1].timestamp) > 0)
            m->records[m->count - 1] = r[j];
        qsort(m->records, m->count, sizeof(nt_record_t), compare);
    }
}
void nt_stats(const nt_model_t *m, bool special, int32_t today, nt_stat_t out[12]) {
    memset(out, 0, 12 * sizeof(*out));
    for (int z = 0; z < 12; z++) {
        out[z].misses = m->count;
        bool streak = true;
        for (size_t i = 0; i < m->count; i++) {
            bool hit = false;
            for (int p = special ? 6 : 0; p < 7 - (special ? 0 : 1); p++)
                if (m->records[i].zodiac[p] == z)
                    hit = true;
            if (hit) {
                out[z].count++;
                if (out[z].misses == m->count)
                    out[z].misses = i;
                if (m->records[i].day >= today - 6 && m->records[i].day <= today)
                    out[z].week++;
                if (streak)
                    out[z].streak++;
            } else
                streak = false;
        }
    }
}
int64_t nt_countdown(int64_t now, bool today_received) {
    int64_t day = (now + 8 * 3600) / 86400;
    int64_t target = day * 86400 + 13 * 3600 + 32 * 60 + (today_received ? 86400 : 0);
    return target > now ? target - now : 0;
}
bool nt_idle_should_blank(int64_t now_us, int64_t last_input_us, unsigned seconds) {
    return seconds && now_us - last_input_us > (int64_t)seconds * 1000000;
}
unsigned nt_history_budget(int seconds_beijing) {
    const int start = 21 * 3600 + 30 * 60, end = 21 * 3600 + 40 * 60;
    if (seconds_beijing >= start && seconds_beijing < end)
        return 0;
    if (seconds_beijing < start && start - seconds_beijing < 120)
        return start - seconds_beijing;
    return 120;
}
unsigned nt_history_timeout_seconds(unsigned budget) {
    return budget < 5 ? budget : 5;
}
unsigned nt_history_retry_interval(unsigned failures) {
    return failures <= 1 ? 30 : failures == 2 ? 60 : 120;
}
bool nt_history_should_sync(bool needed, int64_t now, int64_t retry_due) {
    return needed && now >= retry_due;
}
bool nt_history_should_cancel(bool active, bool manual) {
    return active && manual;
}
unsigned nt_history_retry_wait(unsigned interval, bool needed, int seconds_beijing, int64_t now,
                               int64_t retry_due) {
    if (!needed || !nt_history_budget(seconds_beijing) || retry_due <= now)
        return interval;
    unsigned retry = (unsigned)(retry_due - now);
    return retry < interval ? retry : interval;
}
unsigned nt_poll_interval(int seconds_beijing, bool today_done, bool success, unsigned failures) {
    (void)today_done;
    if (!success)
        return failures <= 1 ? 1 : failures == 2 ? 2 : 5;
    const int start = 21 * 3600 + 30 * 60, end = 21 * 3600 + 40 * 60;
    if (seconds_beijing < start)
        return start - seconds_beijing;
    if (seconds_beijing < end)
        return 0;
    return UINT32_MAX;
}
bool nt_live_merge(nt_live_t *current, const nt_live_t *incoming) {
    if (!incoming->present)
        return false;
    int order = current->present ? strcmp(incoming->record.issue, current->record.issue) : 1;
    if (order < 0 || (order == 0 && incoming->revealed < current->revealed))
        return false;
    nt_live_t merged = *incoming;
    if (order == 0)
        for (int i = 0; i < merged.revealed; i++)
            if (!strcmp(merged.record.number[i], current->record.number[i])) {
                if (!(merged.wave_mask & (1U << i)) && (current->wave_mask & (1U << i))) {
                    merged.wave_mask |= 1U << i;
                    merged.record.wave[i] = current->record.wave[i];
                }
                if (!(merged.zodiac_mask & (1U << i)) && (current->zodiac_mask & (1U << i))) {
                    merged.zodiac_mask |= 1U << i;
                    merged.record.zodiac[i] = current->record.zodiac[i];
                }
            }
    if (!memcmp(current, &merged, sizeof(merged)))
        return false;
    *current = merged;
    return true;
}

int nt_focus_step(int focus, int delta, int first, int last) {
    int next = focus + delta;
    if (next < first)
        return first;
    if (next > last)
        return last;
    return next;
}

bool nt_current_pending(const nt_model_t *m, int64_t now) {
    if (now < 1700000000 || !m->count)
        return false;
    int64_t local = now + 8 * 3600;
    return m->records[0].day < local / 86400 && local % 86400 >= 20 * 3600 + 32 * 60;
}
