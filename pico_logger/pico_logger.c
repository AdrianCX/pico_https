#include <stdarg.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/ip4_addr.h"

#include "logging_config.h"

struct udp_pcb upstream_pcb;

#define BUFFER_SIZE 1000
char buffer[BUFFER_SIZE];

#ifdef __cplusplus
extern "C" {
#endif

bool start_logging_server()
{
    const ip_addr_t any_addr = {0};
    ip_addr_t remote_addr;

    // Yes, it's weird it returns 1 on success and 0 on failure: https://www.nongnu.org/lwip/2_0_x/ip4__addr_8c.html
    if (0 == ip4addr_aton(LOGGING_SERVER, &remote_addr))
    {
        printf("Failed decoding remote logging server address, address: %s\r\n", LOGGING_SERVER);
        return false;
    }

    err_t err = udp_bind(&upstream_pcb, &any_addr, 5000);
    if (err != 0)
    {
        printf("Failed binding upstream on local port, error: %d\r\n", err);
        return false;
    }

    udp_connect(&upstream_pcb, &remote_addr, LOGGING_SERVER_PORT);

    return true;
}

void trace(const char *format, ...)
{
    va_list args;
    va_start (args, format);
    int size = vsnprintf (buffer, BUFFER_SIZE, format, args);
    va_end (args);

    struct pbuf *p = pbuf_alloc( PBUF_TRANSPORT, size, PBUF_RAM);
    memcpy(p->payload, buffer, size);
    
    err_t err = udp_send(&upstream_pcb, p);
    if (err != ERR_OK) {
        printf("Failed to send UDP packet! error=%d", err);
    }
    pbuf_free(p);
}

#ifdef __cplusplus
}
#endif