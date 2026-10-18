#include "ninehalf_stream.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *field(cJSON *o, const char *key) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}
static bool tokens(const char *s, char out[7][16]) {
    if (!s || strlen(s) > 110)
        return false;
    for (int i = 0; i < 7; i++) {
        const char *end = strchr(s, ',');
        size_t len = end ? (size_t)(end - s) : strlen(s);
        if (len == 0 || len >= 16 || (i < 6 && !end) || (i == 6 && end))
            return false;
        memcpy(out[i], s, len);
        out[i][len] = 0;
        s = end ? end + 1 : s + len;
    }
    return true;
}
bool nt_parse_record(const char *json, nt_record_t *out) {
    cJSON *o = cJSON_Parse(json);
    if (!o)
        return false;
    bool valid = false;
    const char *issue = field(o, "expect"), *time = field(o, "openTime");
    char nums[7][16], waves[7][16], zs[7][16];
    if (!issue || !time || strlen(issue) == 0 || strlen(issue) >= sizeof(out->issue) ||
        strlen(time) != 19 || nt_day(time) < 0)
        goto done;
    int hour, minute, second;
    if (time[4] != '-' || time[7] != '-' || time[10] != ' ' || time[13] != ':' || time[16] != ':' ||
        sscanf(time + 11, "%d:%d:%d", &hour, &minute, &second) != 3 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59)
        goto done;
    for (const char *p = issue; *p; p++)
        if (*p < '0' || *p > '9')
            goto done;
    if (!tokens(field(o, "openCode"), nums) || !tokens(field(o, "wave"), waves) ||
        !tokens(field(o, "zodiac"), zs))
        goto done;
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 7; i++) {
        char *end;
        long n = strtol(nums[i], &end, 10);
        if (*end || strlen(nums[i]) > 2 || n < 1 || n > 49)
            goto done;
        int z = nt_zodiac(zs[i]);
        if (z < 0)
            goto done;
        int w = !strcmp(waves[i], "red")     ? 0
                : !strcmp(waves[i], "blue")  ? 1
                : !strcmp(waves[i], "green") ? 2
                                             : -1;
        if (w < 0)
            goto done;
        for (int j = 0; j < i; j++)
            if (atoi(out->number[j]) == n)
                goto done;
        strcpy(out->number[i], nums[i]);
        out->zodiac[i] = z;
        out->wave[i] = w;
    }
    strcpy(out->issue, issue);
    strcpy(out->timestamp, time);
    out->day = nt_day(time);
    valid = true;
done:
    cJSON_Delete(o);
    return valid;
}
static bool partial_tokens(const char *s, char out[7][16], int *count) {
    *count = 0;
    if (!s || strlen(s) > 110)
        return false;
    while (*s && *count < 7) {
        const char *end = strchr(s, ',');
        size_t len = end ? (size_t)(end - s) : strlen(s);
        if (!len) {
            while (*s == ',')
                s++;
            return !*s && *count > 0;
        }
        if (len >= 16)
            return false;
        memcpy(out[*count], s, len);
        out[(*count)++][len] = 0;
        if (!end)
            return true;
        s = end + 1;
    }
    return !*s && *count > 0;
}
bool nt_parse_live(const char *json, nt_live_t *out) {
    cJSON *o = cJSON_Parse(json);
    if (!o)
        return false;
    bool valid = false;
    const char *issue = field(o, "expect"), *time = field(o, "openTime");
    char nums[7][16] = {{0}}, waves[7][16] = {{0}}, zs[7][16] = {{0}};
    int n = 0, wn = 0, zn = 0;
    const char *codes = field(o, "openCode");
    if (!issue || !*issue || strlen(issue) >= sizeof(out->record.issue) ||
        (codes && *codes && !partial_tokens(codes, nums, &n)))
        goto done;
    for (const char *p = issue; *p; p++)
        if (*p < '0' || *p > '9')
            goto done;
    if (field(o, "wave") && *field(o, "wave") &&
        !partial_tokens(field(o, "wave"), waves, &wn))
        goto done;
    if (field(o, "zodiac") && *field(o, "zodiac") &&
        !partial_tokens(field(o, "zodiac"), zs, &zn))
        goto done;
    memset(out, 0, sizeof(*out));
    out->record.day = -1;
    if (time && *time) {
        if (strlen(time) != 19 || nt_day(time) < 0)
            goto done;
        strcpy(out->record.timestamp, time);
        out->record.day = nt_day(time);
    }
    strcpy(out->record.issue, issue);
    for (int i = 0; i < n; i++) {
        char *end;
        long number = strtol(nums[i], &end, 10);
        if (*end || strlen(nums[i]) > 2 || number < 1 || number > 49)
            goto done;
        for (int j = 0; j < i; j++)
            if (atoi(out->record.number[j]) == number)
                goto done;
        strcpy(out->record.number[i], nums[i]);
        if (i < wn) {
            int w = !strcmp(waves[i], "red")     ? 0
                    : !strcmp(waves[i], "blue")  ? 1
                    : !strcmp(waves[i], "green") ? 2
                                                  : -1;
            if (w < 0)
                goto done;
            out->record.wave[i] = w;
            out->wave_mask |= 1U << i;
        }
        if (i < zn) {
            int z = nt_zodiac(zs[i]);
            if (z < 0)
                goto done;
            out->record.zodiac[i] = z;
            out->zodiac_mask |= 1U << i;
        }
    }
    out->revealed = n;
    out->present = true;
    valid = true;
done:
    cJSON_Delete(o);
    return valid;
}
/* Validate JSON tokens while retaining only individual record objects. */
static bool json_value(nt_stream_t *s) {
    if (!s->json_depth) {
        if (s->json_root)
            return false;
        s->json_root = true;
        return true;
    }
    unsigned i = s->json_depth - 1;
    if (s->json_container[i] == '{') {
        if (s->json_state[i] != 2)
            return false;
        s->json_state[i] = 3;
    } else {
        if (s->json_state[i] != 0 && s->json_state[i] != 2)
            return false;
        s->json_state[i] = 1;
    }
    return true;
}
static bool json_primitive(nt_stream_t *s) {
    s->token[s->token_used] = 0;
    const char *p = s->token;
    if (!strcmp(p, "true") || !strcmp(p, "false") || !strcmp(p, "null"))
        return true;
    if (*p == '-')
        p++;
    if (*p == '0')
        p++;
    else {
        if (*p < '1' || *p > '9')
            return false;
        do {
            p++;
        } while (*p >= '0' && *p <= '9');
    }
    if (*p == '.') {
        p++;
        if (*p < '0' || *p > '9')
            return false;
        do {
            p++;
        } while (*p >= '0' && *p <= '9');
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (*p < '0' || *p > '9')
            return false;
        do {
            p++;
        } while (*p >= '0' && *p <= '9');
    }
    return !*p;
}
static bool json_byte(nt_stream_t *s, unsigned char c) {
    if (s->json_lex == 1) {
        if (s->unicode_left) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                return false;
            s->unicode_left--;
            return true;
        }
        if (s->json_escape) {
            s->json_escape = false;
            if (c == 'u')
                s->unicode_left = 4;
            else if (!strchr("\"\\/bfnrt", c))
                return false;
            return true;
        }
        if (c < 32)
            return false;
        if (c == '\\')
            s->json_escape = true;
        else if (c == '"') {
            s->json_lex = 0;
            if (s->json_key)
                s->json_state[s->json_depth - 1] = 1;
        }
        return true;
    }
    if (s->json_lex == 2) {
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t' && c != ',' && c != ']' && c != '}') {
            if (s->token_used + 1 >= sizeof(s->token))
                return false;
            s->token[s->token_used++] = c;
            return true;
        }
        if (!json_primitive(s))
            return false;
        s->json_lex = 0;
    }
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
        return true;
    if (!s->json_depth && s->json_root)
        return false;
    unsigned i = s->json_depth ? s->json_depth - 1 : 0;
    if (c == '"') {
        s->json_key = s->json_depth && s->json_container[i] == '{' &&
                      (s->json_state[i] == 0 || s->json_state[i] == 4);
        if (!s->json_key && !json_value(s))
            return false;
        s->json_lex = 1;
        return true;
    }
    if (c == '{' || c == '[') {
        if (!json_value(s) || s->json_depth == 16)
            return false;
        s->json_container[s->json_depth] = c;
        s->json_state[s->json_depth++] = 0;
        return true;
    }
    if (c == '}' || c == ']') {
        if (!s->json_depth || s->json_container[i] != (c == '}' ? '{' : '['))
            return false;
        if (c == '}' ? (s->json_state[i] != 0 && s->json_state[i] != 3)
                     : (s->json_state[i] != 0 && s->json_state[i] != 1))
            return false;
        s->json_depth--;
        return true;
    }
    if (c == ':') {
        if (!s->json_depth || s->json_container[i] != '{' || s->json_state[i] != 1)
            return false;
        s->json_state[i] = 2;
        return true;
    }
    if (c == ',') {
        if (!s->json_depth)
            return false;
        if (s->json_container[i] == '{') {
            if (s->json_state[i] != 3)
                return false;
            s->json_state[i] = 4;
        } else {
            if (s->json_state[i] != 1)
                return false;
            s->json_state[i] = 2;
        }
        return true;
    }
    if (!(c == 't' || c == 'f' || c == 'n' || c == '-' || (c >= '0' && c <= '9')) || !json_value(s))
        return false;
    s->json_lex = 2;
    s->token_used = 1;
    s->token[0] = c;
    return true;
}

void nt_stream_feed(nt_stream_t *s, const char *data, size_t length) {
    if (s->failed)
        return;
    s->total += length;
    if (s->total > 512 * 1024) {
        s->failed = true;
        return;
    }
    for (size_t i = 0; i < length; i++) {
        char c = data[i];
        if (!json_byte(s, (unsigned char)c)) {
            s->failed = true;
            return;
        }
        if (!s->quote) {
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            } else if (s->finished) {
                s->failed = true;
                return;
            } else if (c == '{' || c == '[') {
                if (s->depth == 16) {
                    s->failed = true;
                    return;
                }
                s->containers[s->depth++] = c;
                s->started = true;
            } else if (c == '}' || c == ']') {
                if (!s->depth || s->containers[s->depth - 1] != (c == '}' ? '{' : '[')) {
                    s->failed = true;
                    return;
                }
                if (--s->depth == 0)
                    s->finished = true;
            } else if (!s->started) {
                s->failed = true;
                return;
            }
        }
        if (!s->quote && c == '{') {
            s->used = 0;
            s->overflow = false;
        }
        if (s->used || (!s->quote && c == '{')) {
            if (s->used + 1 < sizeof(s->object))
                s->object[s->used++] = c;
            else
                s->overflow = true;
        }
        if (s->quote) {
            if (s->escape)
                s->escape = false;
            else if (c == '\\')
                s->escape = true;
            else if (c == '"')
                s->quote = false;
        } else if (c == '"')
            s->quote = true;
        else if (c == '}' && s->used) {
            s->object[s->used] = 0;
            nt_record_t r;
            if (!s->overflow && nt_parse_record(s->object, &r)) {
                nt_merge_records(&s->result, &r, 1);
                s->records++;
            } else if (!s->overflow && strstr(s->object, "\"expect\"")) {
                nt_live_t live;
                if (!nt_parse_live(s->object, &live)) {
                    s->failed = true;
                    return;
                }
                if (!s->live.present || strcmp(live.record.issue, s->live.record.issue) >= 0)
                    s->live = live;
            } else if (strstr(s->object, "\"expect\"")) {
                s->failed = true;
                return;
            }
            s->used = 0;
        }
    }
}

bool nt_stream_finish(const nt_stream_t *s) {
    return !s->failed && !s->json_lex && !s->json_depth && s->json_root && s->started &&
           s->finished && !s->depth && !s->quote && !s->used &&
           (s->result.count || s->live.present);
}
