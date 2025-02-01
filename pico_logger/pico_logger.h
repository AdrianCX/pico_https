#ifndef _PICO_LOGGER_H
#define _PICO_LOGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize logging library. Provided parameters must be statically allocated.
int start_logging_server(const char *binary_name, const char *hardware_name, const char *software_version);
    
void trace(const char *parameters, ...);
void trace_bytes(uint8_t *buffer, uint32_t size);
void fail(const char *parameters, ...);
void trace_stack();
void force_restart();

const char *safestr(const char *value);
    
#ifdef __cplusplus
}
#endif


#endif
