#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set pico in deep sleep with everything off, GPIO state is maintained.
// soft reboots once sleep is finished.
//
// returns false if any failurer going to sleep.
//
bool pico_deepsleep(uint64_t sleep_time_ms);

#ifdef __cplusplus
}
#endif


