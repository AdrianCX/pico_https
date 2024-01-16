#ifndef _PICO_LOGGER_H
#define _PICO_LOGGER_H


#ifdef __cplusplus
extern "C" {
#endif

bool start_logging_server();
void trace(const char *parameters, ...);
    
#ifdef __cplusplus
}
#endif


#endif