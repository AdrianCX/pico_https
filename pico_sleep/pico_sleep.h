#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize powman for deep sleep with given sleep_time_ms.
bool pico_deepsleep_init(uint64_t sleep_time_ms);

// configure a gpio wakeup
void pico_deepsleep_set_gpio(int hw_instance, uint8_t gpio, bool edge, bool high);

// Go to sleep with given configuration
bool pico_deepsleep_execute();

// Check if wakeup was due to a gpio from given hw_instance
bool pico_deepsleep_wake_reason_gpio(uint8_t hw_instance);

// Full wrapper to go to sleep with given sleep_time_ms.
bool pico_deepsleep(uint64_t sleep_time_ms);

uint8_t pico_deepsleep_wake_reason();
    
#ifdef __cplusplus
}
#endif


