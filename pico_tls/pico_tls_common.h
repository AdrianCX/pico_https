#ifndef _PICO_TLS_COMMON_H
#define _PICO_TLS_COMMON_H

#if PICO_TLS_DEBUG == LWIP_DBG_OFF
#define log(...) do {} while (0)
#else
#define log(...) printf(__VA_ARGS__)
#endif

#endif