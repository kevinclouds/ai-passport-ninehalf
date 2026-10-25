#include "ninehalf_wifi.h"
#include <string.h>
unsigned nt_wifi_scan_interval(unsigned failures) {
    if (failures <= 3) return 10;
    if (failures <= 6) return 30;
    return 60;
}
void nt_wifi_remember(nt_wifi_profiles_t *p, const char *ssid, const char *password) {
    if (!ssid[0])
        return;
    int index = 0;
    while (index < p->count && strcmp(p->items[index].ssid, ssid))
        index++;
    if (index == p->count) {
        if (p->count < NT_WIFI_LIMIT)
            p->count++;
        else
            index = NT_WIFI_LIMIT - 1;
    }
    nt_wifi_profile_t item = {0};
    strncpy(item.ssid, ssid, 32);
    strncpy(item.password, password, 64);
    memmove(p->items + 1, p->items, index * sizeof(item));
    p->items[0] = item;
}
bool nt_wifi_forget(nt_wifi_profiles_t *p, const char *ssid) {
    int i = 0;
    while (i < p->count && strcmp(p->items[i].ssid, ssid))
        i++;
    if (i == p->count)
        return false;
    memmove(p->items + i, p->items + i + 1, (p->count - i - 1) * sizeof(p->items[0]));
    memset(&p->items[--p->count], 0, sizeof(p->items[0]));
    return true;
}
int nt_wifi_best(const nt_wifi_profiles_t *p, const int8_t signal[NT_WIFI_LIMIT], uint8_t tried) {
    int best = -1;
    for (int i = 0; i < p->count; i++)
        if (signal[i] != INT8_MIN && !(tried & (1U << i)) && (best < 0 || signal[i] > signal[best]))
            best = i;
    return best;
}
