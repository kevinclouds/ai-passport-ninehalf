#pragma once
#include "ninehalf_model.h"
typedef struct {
    char object[2048];
    size_t used, total;
    bool quote, escape, overflow, failed;
    unsigned records, depth;
    char containers[16];
    bool started, finished;
    unsigned json_depth, json_lex, unicode_left;
    unsigned char json_state[16];
    char json_container[16], token[64];
    size_t token_used;
    bool json_root, json_key, json_escape;
    nt_model_t result;
} nt_stream_t;
void nt_stream_feed(nt_stream_t *s, const char *data, size_t length);
bool nt_parse_record(const char *json, nt_record_t *out);
bool nt_stream_finish(const nt_stream_t *s);
