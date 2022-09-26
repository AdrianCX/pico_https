#include "mbedtls_wrapper.h"
#include "arch/cc.h"

/* Function to feed mbedtls entropy. May be better to move it to pico-sdk */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    for(size_t p=0; p<len; p++) {
        output[p]=pico_lwip_rand();
    }

    *olen = len;
    return 0;
}

#if defined(MBEDTLS_DEBUG_C)
void mbedtls_debug_print(void *cookie, int level, const char *file, int line, const char *message) {
    printf("%s:%d %s\r\n", file, line, message);
}
#endif

