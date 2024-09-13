#ifndef MBEDTLS_WRAPPER_H
#define MBEDTLS_WRAPPER_H

#include "pico/cyw43_arch.h"

#ifdef __cplusplus
extern "C" {
#endif
    
int sha1(const unsigned char *input, size_t ilen, unsigned char output[20]);
int base64_encode(const unsigned char *src, size_t slen, unsigned char *dst, size_t dlen);

void mbedtls_debug_print(void *cookie, int level, const char *file, int line, const char *message);
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);

#ifdef __cplusplus
}
#endif

#endif
