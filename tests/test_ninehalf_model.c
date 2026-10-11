#include "ninehalf_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    nt_model_t m = {0};
    nt_record_t a = {.issue = "2026277", .timestamp = "2026-10-04 21:32:32", .day = 20730};
    for (int i = 0; i < 7; i++) {
        strcpy(a.number[i], "02");
        a.zodiac[i] = 8;
    }
    nt_merge_records(&m, &a, 1);
    nt_merge_records(&m, &a, 1);
    assert(m.count == 1);
    assert(!strcmp(m.records[0].number[0], "02"));
    nt_stat_t out[12];
    nt_stats(&m, false, 20730, out);
    assert(out[8].count == 1 && out[8].misses == 0 && out[8].streak == 1 && out[8].week == 1);
    assert(out[0].misses == 1);
    nt_record_t b = a;
    strcpy(b.issue, "2026276");
    strcpy(b.timestamp, "2026-10-03 21:32:32");
    b.day--;
    for (int i = 0; i < 7; i++)
        b.zodiac[i] = 0;
    nt_merge_records(&m, &b, 1);
    assert(!strcmp(m.records[0].issue, a.issue));
    nt_stats(&m, true, 20730, out);
    assert(out[0].misses == 1 && out[0].streak == 0);
    assert(out[8].count == 1);
    nt_stats(&m, false, 20737, out);
    assert(out[8].week == 0);
    assert(nt_zodiac("马") == 6 && nt_zodiac("馬") == 6 && nt_zodiac("invalid") == -1);
    int64_t now = 17700 * 86400 + 13 * 3600 + 30 * 60;
    assert(nt_countdown(now, false) == 0);
    assert(nt_countdown(now, true) == 86400);
    assert(nt_focus_step(0, -1, -1, 28) == -1);
    assert(nt_focus_step(-1, -1, -1, 28) == -1);
    assert(nt_focus_step(28, 1, -1, 28) == 28);
    assert(nt_focus_step(-1, -1, -2, 11) == -2);
    assert(nt_day("2026-10-04 21:32:32") == 20730);
    assert(nt_day("2026-02-30") == -1);
    assert(nt_day("2024-02-29") >= 0);
    int64_t cutoff = 20731LL * 86400 + 12 * 3600 + 30 * 60;
    assert(!nt_current_pending(&m, cutoff - 1));
    assert(nt_current_pending(&m, cutoff));
    assert(nt_current_pending(&m, cutoff + 7200));
    assert(!nt_current_pending(&m, 0));
    m.records[0].day = 20731;
    assert(!nt_current_pending(&m, cutoff));
    puts("ninehalf model: PASS");
}
