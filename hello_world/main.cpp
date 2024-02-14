#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "tls_listener.h"
#include "request_handler.h"

#include "wifi.h"
#include "pico_logger.h"

int main() {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    cyw43_arch_disable_ap_mode();

    int err_code = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);

    if (err_code != 0)
    {
        for (int i=0;i<5;++i)
        {
            printf("[%d] failed to connect to wifi, ssid[%s], password[%s] err[%d], restarting in %d seconds\n", i, WIFI_SSID, WIFI_PASSWORD, err_code, 5-i);
            sleep_ms(1000);
        }

        #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
        AIRCR_Register = 0x5FA0004;
        return 1;
    }
    
    start_logging_server();
    
    trace("Starting");
    
    TLSListener *client = new TLSListener();
    client->listen(443, RequestHandler::create);

    // Constantly print a message so we can see this in minicom over usb
    while (true) {
        trace("currently active: %s\r\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
        sleep_ms(5000);
    }

    cyw43_arch_deinit();
    return 0;
}

