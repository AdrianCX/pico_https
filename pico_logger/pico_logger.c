#include <stdarg.h>

#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"

#include "hardware/exception.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
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

#define ALARM_NUM 0
#define ALARM_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM)

#define MAGIC_CALLSTACK 0xdeadbeef
uint32_t __uninitialized_ram(last_command);
uint32_t __uninitialized_ram(cur_depth);
uint32_t __uninitialized_ram(call_stack_buf)[CMB_CALL_STACK_MAX_DEPTH];

uint32_t watchdog_timeout_us = 0;
                                            
#ifdef __cplusplus
extern "C" {
#endif

void __attribute__((used,naked)) HardFault_Handler(void);

static void watchdog_timeout(void) {
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    // save the callstack
    fault_init(cmb_get_sp());

    // restart
    #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

const char *safestr(const char *value)
{
    return value != NULL ? value : "null";
}

int start_logging_server(const char *binary_name, const char *hardware_name, const char *software_version, uint32_t watchdog_ms)
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

    if (watchdog_ms != 0)
    {
        watchdog_timeout_us = watchdog_ms * 1000;

        // Use a regular IRQ so we can save call stack
        hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
        irq_set_exclusive_handler(ALARM_IRQ, watchdog_timeout);
        irq_set_enabled(ALARM_IRQ, true);
        timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + watchdog_timeout_us;

        // start watchdog just in case everything else fails 1 second after a IRQ would trigger
        //watchdog_enable(watchdog_ms + 1000, 1);
    }
    
    report_saved_crash();
    return 1;
}

void update_watchdog()
{
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + watchdog_timeout_us;
    //watchdog_update();
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

void trace_bytes(const uint8_t *buffer, uint32_t size)
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
    fault_init(cmb_get_sp());
    
    // restart
    #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

void pico_logger_panic(const char *parameters, ...)
{
    // save call stack
    fault_init(cmb_get_sp());

    // restart
    #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

void trace_stack()
{
    cm_backtrace_assert(cmb_get_sp());
}

void force_restart()
{
    for (int i=0;i<3;i++)
    {
        busy_wait_us(1000000);
    }
    
    #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

void fault_init(uint32_t fault_handler_sp)
{
    // save crash for sending on restart as well.
    cur_depth = 0;
    last_command = MAGIC_CALLSTACK;
    cur_depth = cm_backtrace_call_stack(call_stack_buf, CMB_CALL_STACK_MAX_DEPTH, fault_handler_sp);
}

void report_saved_crash()
{
    if (last_command != MAGIC_CALLSTACK)
    {
        last_command = 0;
        return;
    }

    if (cur_depth > CMB_CALL_STACK_MAX_DEPTH || cur_depth == 0)
    {
        // corrupted ram data
        return;
    }

    trace("report_saved_crash: last boot info, watchdog: %d/%d, last_command: 0x%x, cur_depth: %d", watchdog_caused_reboot(), watchdog_enable_caused_reboot(), last_command, cur_depth);
    cm_print_call_stack(call_stack_buf, cur_depth);
}
    
#ifdef __cplusplus
}
#endif
