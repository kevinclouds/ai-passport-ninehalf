#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NT_RECORDS 30
#define NT_ZODIACS 12
typedef struct {
    char issue[20], timestamp[20], number[7][3];
    int32_t day;
    uint8_t wave[7], zodiac[7];
} nt_record_t;
typedef struct {
    nt_record_t record;
    uint8_t revealed, wave_mask, zodiac_mask;
    bool present;
} nt_live_t;
typedef struct {
    uint8_t count, week, misses, streak;
} nt_stat_t;
typedef struct {
    uint8_t volume, brightness;
    bool reminders;
    char notified[20];
} nt_settings_t;
typedef struct {
    nt_record_t records[NT_RECORDS];
    size_t count;
    nt_settings_t settings;
} nt_model_t;
void nt_merge_records(nt_model_t *m, const nt_record_t *records, size_t count);
void nt_stats(const nt_model_t *m, bool special, int32_t today, nt_stat_t out[12]);
int nt_zodiac(const char *name);
int32_t nt_day(const char *timestamp);
int64_t nt_countdown(int64_t now, bool today_received);
bool nt_idle_should_blank(int64_t now_us, int64_t last_input_us, unsigned seconds);
unsigned nt_history_budget(int seconds_beijing);
unsigned nt_history_timeout_seconds(unsigned budget);
unsigned nt_history_retry_interval(unsigned failures);
bool nt_history_should_sync(bool needed, int64_t now, int64_t retry_due);
bool nt_history_should_cancel(bool active, bool manual);
unsigned nt_history_retry_wait(unsigned interval, bool needed, int seconds_beijing, int64_t now,
                               int64_t retry_due);
unsigned nt_poll_interval(int seconds_beijing, bool today_done, bool success, unsigned failures);
unsigned nt_network_wait_ms(bool online, bool was_live, int seconds_beijing, int64_t now,
                            int64_t due, unsigned offline_wait);
bool nt_live_merge(nt_live_t *current, const nt_live_t *incoming);
extern const char *const nt_zodiac_names[12];

int nt_focus_step(int focus, int delta, int first, int last);

bool nt_current_pending(const nt_model_t *m, int64_t now);
