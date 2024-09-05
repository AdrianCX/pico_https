#ifndef _PICO_LOGGER_H
#define _PICO_LOGGER_H


#ifdef __cplusplus
extern "C" {
#endif

int start_logging_server();
void trace(const char *parameters, ...);
void fail(const char *parameters, ...);
    
const char *safestr(const char *value);
    
#ifdef __cplusplus
}
#endif


#endif
