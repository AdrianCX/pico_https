/* MIT License

Copyright (c) 2024 Adrian Cruceru - https://github.com/AdrianCX/pico_https

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
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

void mbedtls_debug_print(void *cookie, int level, const char *file, int line, const char *message) {
    trace("%s:%d %s\r\n", file, line, message);
}

#ifdef __cplusplus
}
#endif
