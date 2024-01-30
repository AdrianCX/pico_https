#include "mbedtls_wrapper.h"
#include "arch/cc.h"

#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"
#include "pico_tls_common.h"

#ifdef __cplusplus
extern "C" {
#endif
    
int sha1(const unsigned char *input, size_t ilen, unsigned char output[20])
{
    return mbedtls_sha1_ret(input, ilen, output);
}

int base64_encode(const unsigned char *src, size_t slen, unsigned char *dst, size_t dlen)
{
    size_t olen;

    int r = mbedtls_base64_encode(dst, dlen - 1, &olen, src, slen);
    if (0 != r) {
        return r;
    }
    
    dst[olen] = 0;
    return 0;
}

#if defined(MBEDTLS_DEBUG_C)
void mbedtls_debug_print(void *cookie, int level, const char *file, int line, const char *message) {
    trace("%s:%d %s\r\n", file, line, message);
}
#endif

#ifdef __cplusplus
}
#endif
