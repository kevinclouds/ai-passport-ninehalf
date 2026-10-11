#include "ninehalf_wifi.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    nt_wifi_profiles_t p = {0};
    nt_wifi_remember(&p, "Home", "old");
    nt_wifi_remember(&p, "Office", "work");
    nt_wifi_remember(&p, "Home", "new");
    assert(p.count == 2 && !strcmp(p.items[0].password, "new"));
    int8_t signals[5] = {-70, -40, INT8_MIN, INT8_MIN, INT8_MIN};
    assert(nt_wifi_best(&p, signals, 0) == 1);
    assert(nt_wifi_best(&p, signals, 2) == 0);
    assert(nt_wifi_best(&p, signals, 3) == -1);
    nt_wifi_remember(&p, "A", "a");
    nt_wifi_remember(&p, "B", "b");
    nt_wifi_remember(&p, "C", "c");
    nt_wifi_remember(&p, "D", "d");
    assert(p.count == 5 && !strcmp(p.items[4].ssid, "Home"));
    assert(!nt_wifi_forget(&p, "Office"));
    assert(nt_wifi_forget(&p, "Home"));
    assert(p.count == 4 && !p.items[4].ssid[0]);
    while (p.count)
        assert(nt_wifi_forget(&p, p.items[0].ssid));
    assert(nt_wifi_best(&p, signals, 0) == -1);
    puts("ninehalf Wi-Fi profiles: PASS");
}
