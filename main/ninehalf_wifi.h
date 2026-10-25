#pragma once
#include <stdbool.h>
#include <stdint.h>
#define NT_WIFI_LIMIT 5
typedef struct {
    char ssid[33], password[65];
} nt_wifi_profile_t;
typedef struct {
    uint8_t count;
    nt_wifi_profile_t items[NT_WIFI_LIMIT];
} nt_wifi_profiles_t;
typedef struct {
    uint8_t count;
    char names[NT_WIFI_LIMIT][33], current[33];
    int8_t signal[NT_WIFI_LIMIT], current_signal;
    bool online, pending;
} nt_wifi_view_t;
void nt_wifi_remember(nt_wifi_profiles_t *p, const char *ssid, const char *password);
bool nt_wifi_forget(nt_wifi_profiles_t *p, const char *ssid);
int nt_wifi_best(const nt_wifi_profiles_t *p, const int8_t signal[NT_WIFI_LIMIT], uint8_t tried);
void nt_wifi_view(nt_wifi_view_t *view);
void nt_wifi_request(const char *ssid, bool remove);
void nt_wifi_manage(void);
unsigned nt_wifi_scan_interval(unsigned failures);
// Network worker owns these operations; never call from button callbacks.
void nt_wifi_expedite(void);
void nt_wifi_power_save(bool idle);
unsigned nt_wifi_retry_wait(void);
