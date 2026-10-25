#include "ninehalf_power.h"
#include "esp_pm.h"
#include "esp_err.h"

static esp_pm_lock_handle_t performance;

void nt_power_init(void) {
    const esp_pm_config_t config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 80,
        // ADC ladder keys require periodic sampling; retain the proven wake path.
        .light_sleep_enable = false,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&config));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "ninehalf", &performance));
}

void nt_power_busy(bool busy) {
    if (busy)
        ESP_ERROR_CHECK(esp_pm_lock_acquire(performance));
    else
        ESP_ERROR_CHECK(esp_pm_lock_release(performance));
}
