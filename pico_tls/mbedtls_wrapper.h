#ifndef MBEDTLS_WRAPPER_H
#define MBEDTLS_WRAPPER_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#if defined(MBEDTLS_DEBUG_C)
void mbedtls_debug_print(void *cookie, int level, const char *file, int line, const char *message);
#endif

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);

#endif