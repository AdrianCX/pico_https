#ifndef _PICO_TLS_COMMON_H
#define _PICO_TLS_COMMON_H

#if PICO_TLS_DEBUG == LWIP_DBG_OFF
#define trace(...) do {} while (0)
#else
#include "pico_logger.h"
#endif

#endif