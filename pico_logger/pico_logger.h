#ifndef _PICO_LOGGER_H
#define _PICO_LOGGER_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize logging library. Provided parameters must be statically allocated.
int start_logging_server(const char *binary_name, const char *hardware_name, const char *software_version, uint32_t watchdog_ms);
void update_watchdog();
void trace(const char *parameters, ...);
void trace_bytes(const uint8_t *buffer, uint32_t size);
void fail(const char *parameters, ...);
void trace_stack();
void fault_init(uint32_t fault_handler_sp);
void force_restart();
void report_saved_crash();
void pico_logger_panic(const char *parameters, ...);
const char *safestr(const char *value);
    
#ifdef __cplusplus
}
#endif


#endif
