#pragma once
#include "ninehalf_model.h"
#include "esp_err.h"
esp_err_t nt_wifi_start(void);
bool nt_wifi_online(void);
void nt_wifi_retry(void);
void nt_network_wifi_changed(void);
esp_err_t nt_provision_start(void);
void nt_provision_stop(void);
bool nt_provision_active(void);
bool nt_provision_complete(void);
void nt_network_start(void);
void nt_network_refresh(void);
void nt_network_provision(void);
void nt_app_network_result(const nt_model_t *snapshot, bool success);
void nt_app_history_result(const nt_model_t *snapshot);
void nt_app_live_result(const nt_live_t *live);
bool nt_provision_failed(void);

void nt_app_sync_started(void);
void nt_app_sync_finished(bool success);

bool nt_provision_pending(void);
