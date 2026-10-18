#pragma once
#include "ninehalf_model.h"
void nt_store_load(nt_model_t *m);
void nt_store_save(const nt_model_t *m);

unsigned nt_store_sleep_seconds(void);
unsigned nt_store_sleep_delay(void);
void nt_store_sleep_set(bool enabled, unsigned seconds);
