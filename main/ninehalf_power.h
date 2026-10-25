#pragma once
#include <stdbool.h>

void nt_power_init(void);
// Balanced, nestable calls from application tasks; keep interactive work at 160 MHz.
void nt_power_busy(bool busy);
