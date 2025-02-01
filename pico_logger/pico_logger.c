#include <stdarg.h>

#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"

#include "hardware/exception.h"
#include "hardware/watchdog.h"

#include "logging_config.h"
#include "pico_logger.h"
#include "cm_backtrace/cm_backtrace.h"

#ifndef LOGGING_SERVER_BINARY_PORT
#define LOGGING_SERVER_BINARY_PORT LOGGING_SERVER_PORT
#endif

struct udp_pcb upstream_pcb;
struct udp_pcb binary_upstream_pcb;

#define BUFFER_SIZE 1500
char buffer[BUFFER_SIZE] = {0};

#define ADDRESS_SIZE 46
char address[ADDRESS_SIZE] = {0};

// So we can see order and when logs go missing, counter on receiver will skip a few numbers
int log_count = 0;

#ifdef __cplusplus
extern "C" {
#endif

void __attribute__((used,naked)) HardFault_Handler(void);
void __attribute__((used,naked)) Nop_Handler(void) {};

const char *safestr(const char *value)
{
    return value != NULL ? value : "null";
}

int start_logging_server(const char *binary_name, const char *hardware_name, const char *software_version)
{
    cm_backtrace_init(binary_name, hardware_name, software_version);
    
    const ip_addr_t any_addr = {0};
    ip_addr_t remote_addr;

    // Yes, it's weird it returns 1 on success and 0 on failure: https://www.nongnu.org/lwip/2_0_x/ip4__addr_8c.html
    if (0 == ip4addr_aton(LOGGING_SERVER, &remote_addr))
    {
        printf("Failed decoding remote logging server address, address: %s\r\n", LOGGING_SERVER);
        return 0;
    }

    err_t err = udp_bind(&upstream_pcb, &any_addr, 5000);
    if (err != 0)
    {
        printf("Failed binding upstream on local port, error: %d\r\n", err);
        return 0;
    }

    udp_connect(&upstream_pcb, &remote_addr, LOGGING_SERVER_PORT);

    err = udp_bind(&binary_upstream_pcb, &any_addr, 5001);
    if (err != 0)
    {
        printf("Failed binding upstream on local port, error: %d\r\n", err);
        return 0;
    }

    udp_connect(&binary_upstream_pcb, &remote_addr, LOGGING_SERVER_BINARY_PORT);
    
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION,HardFault_Handler);
    exception_set_exclusive_handler(SVCALL_EXCEPTION,HardFault_Handler);
    exception_set_exclusive_handler(PENDSV_EXCEPTION,HardFault_Handler);
    exception_set_exclusive_handler(NMI_EXCEPTION, HardFault_Handler);

    strncpy(address, ip4addr_ntoa(netif_ip4_addr(netif_list)), ADDRESS_SIZE-1);
    
    return 1;
}

static void send_message(const char *category, const char *format, va_list args)
{
    int time = to_us_since_boot(get_absolute_time())/1000;

    int size = snprintf(buffer, BUFFER_SIZE, "%3d %8d %s %s ", log_count, time, address, category);

    if (size < BUFFER_SIZE-1)
    {
        log_count = (log_count+1)%999;
        
        size += vsnprintf(&buffer[size], BUFFER_SIZE-size-1, format, args);

        if (size < BUFFER_SIZE-1)
        {
            if (buffer[size-1] != '\n')
            {
                buffer[size] = '\n';
                buffer[++size] = 0;
            }
        }

        if (size > BUFFER_SIZE)
        {
            buffer[BUFFER_SIZE-1] = 0;
            size = BUFFER_SIZE;
        }
    }

    struct pbuf *p = pbuf_alloc( PBUF_TRANSPORT, size, PBUF_RAM);
    memcpy(p->payload, buffer, size);
    
    err_t err = udp_send(&upstream_pcb, p);
    if (err != ERR_OK) {
        printf("Failed to send UDP packet! error=%d", err);
    }
    pbuf_free(p);
}

void trace(const char *format, ...)
{
    va_list args;
    va_start (args, format);
    send_message("trace", format, args);
    va_end (args);
}

void trace_bytes(uint8_t *buffer, uint32_t size)
{
    struct pbuf *p = pbuf_alloc( PBUF_TRANSPORT, size, PBUF_RAM);
    memcpy(p->payload, buffer, size);
    
    err_t err = udp_send(&binary_upstream_pcb, p);
    if (err != ERR_OK) {
        printf("Failed to send UDP packet! error=%d", err);
    }
    pbuf_free(p);
}


void fail(const char *format, ...)
{
    va_list args;
    va_start (args, format);
    send_message("fail", format, args);
    va_end (args);
}

void trace_stack()
{
    cm_backtrace_assert(cmb_get_sp());
}

void force_restart()
{
    for (int i=0;i<5;i++)
    {
        trace("force_restart: waiting for %d seconds", 5-i);
        busy_wait_us(1000000);
    }
    
    #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

    
#ifdef __cplusplus
}
#endif
