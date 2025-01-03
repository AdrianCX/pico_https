#include <stdarg.h>

#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"

#include "hardware/exception.h"
#include "hardware/watchdog.h"

#include "logging_config.h"
#include "pico_logger.h"
#include "m0FaultDispatch/m0FaultDispatch.h"

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

const char *safestr(const char *value)
{
    return value != NULL ? value : "null";
}

int start_logging_server()
{
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
    

void m0FaultHandle(struct M0excFrame *exc, struct M0highRegs *hiRegs, enum M0faultReason reason, uint32_t addr){
	static const char *names[] = {
		[M0faultMemAccessFailR] = "Memory read failed",
		[M0faultMemAccessFailW] = "Memory write failed",
		[M0faultUnalignedAccess] = "Data alignment fault",
		[M0faultInstrFetchFail] = "Instr fetch failure",
		[M0faultUnalignedPc] = "Code alignment fault",
		[M0faultUndefInstr16] = "Undef 16-bit instr",
		[M0faultUndefInstr32] = "Undef 32-bit instr",
		[M0faultBkptInstr] = "Breakpoint hit",
		[M0faultArmMode] = "ARM mode entered",
		[M0faultUnclassifiable] = "Unclassified fault",
	};

	trace("%s sr = 0x%08lx\r\n", (reason<sizeof(names)/sizeof(*names) && names[reason])? names[reason] : "????",exc->sr);
	trace("R0  = 0x%08lx  R8  = 0x%08lx\r\n", exc->r0_r3[0], hiRegs->r8_r11[0]);
	trace("R1  = 0x%08lx  R9  = 0x%08lx\r\n", exc->r0_r3[1], hiRegs->r8_r11[1]);
	trace("R2  = 0x%08lx  R10 = 0x%08lx\r\n", exc->r0_r3[2], hiRegs->r8_r11[2]);
	trace("R3  = 0x%08lx  R11 = 0x%08lx\r\n", exc->r0_r3[3], hiRegs->r8_r11[3]);
	trace("R4  = 0x%08lx  R12 = 0x%08lx\r\n", hiRegs->r4_r7[0], exc->r12);
	trace("R5  = 0x%08lx  SP  = 0x%08lx\r\n", hiRegs->r4_r7[1], (uint32_t)(exc + 1));
	trace("R6  = 0x%08lx  LR  = 0x%08lx\r\n", hiRegs->r4_r7[2], exc->lr);
	trace("R7  = 0x%08lx  PC  = 0x%08lx\r\n", hiRegs->r4_r7[3], exc->pc);

    switch (reason) {
        case M0faultMemAccessFailR: trace(" -> failed to read 0x%08lx\r\n", addr); break;
        case M0faultMemAccessFailW: trace(" -> failed to write 0x%08lx\r\n", addr); break;
        case M0faultUnalignedAccess: trace(" -> unaligned access to 0x%08lx\r\n", addr); break;
        case M0faultUndefInstr16: trace(" -> undef instr16: 0x%04x\r\n", ((uint16_t*)exc->pc)[0]); break;
        case M0faultUndefInstr32: trace(" -> undef instr32: 0x%04x 0x%04x\r\n", ((uint16_t*)exc->pc)[0], ((uint16_t*)exc->pc)[1]);
        default:
            break;
    }
    
    for (int i=0;i<10;++i)
    {
        trace("crashed at: 0x%08lx\n", exc->pc);
        sleep_ms(1000);
    }

    #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

     
    

#ifdef __cplusplus
}
#endif
