#pragma once
#include "ninehalf_model.h"
#include "bsp_button.h"
void nt_ui_create(nt_model_t *model);
void nt_ui_render(void);
void nt_ui_key(bsp_btn_t button, bsp_btn_ev_t event);
void nt_ui_clock(void);
void nt_ui_sync_begin(void);
void nt_ui_sync_state(bool success);
void nt_ui_notify(void);
void nt_ui_set_live(const nt_live_t *live);
