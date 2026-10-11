#include "ninehalf_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *r = "{\"expect\":\"2026277\",\"openTime\":\"2026-10-04 "
                    "21:32:32\",\"openCode\":\"41,23,14,31,13,32,11\",\"wave\":\"blue,red,blue,"
                    "blue,red,green,green\",\"zodiac\":\"虎,猴,蛇,鼠,馬,豬,猴\"}";
    nt_stream_t s = {0};
    nt_stream_feed(&s, "{\"data\":[", 9);
    for (size_t i = 0; i < strlen(r); i++)
        nt_stream_feed(&s, r + i, 1);
    nt_stream_feed(&s, "]}", 2);
    assert(nt_stream_finish(&s));
    assert(s.result.count == 1);
    assert(s.result.records[0].zodiac[6] == 8);
    nt_stat_t stats[12];
    nt_stats(&s.result, false, s.result.records[0].day, stats);
    assert(stats[8].count == 1);
    nt_record_t record;
    assert(!nt_parse_record("{}", &record));
    assert(!nt_parse_record("{\"expect\":\"too long\"}", &record));
    nt_stream_t truncated = {0};
    nt_stream_feed(&truncated, "[", 1);
    nt_stream_feed(&truncated, r, strlen(r));
    assert(!nt_stream_finish(&truncated));
    nt_stream_feed(&truncated, "]garbage", 8);
    assert(!nt_stream_finish(&truncated));
    const char *bad[] = {" garbage]", ",,]", ",]", " true]"};
    for (unsigned i = 0; i < 4; i++) {
        nt_stream_t invalid = {0};
        nt_stream_feed(&invalid, "[", 1);
        nt_stream_feed(&invalid, r, strlen(r));
        nt_stream_feed(&invalid, bad[i], strlen(bad[i]));
        assert(!nt_stream_finish(&invalid));
    }
    puts("ninehalf stream: PASS");
}
