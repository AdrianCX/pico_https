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

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        int i=0;
        while (true) {
            printf("[%d] failed to connect to wifi, ssid: %s, password: %s\n", i++, WIFI_SSID, WIFI_PASSWORD);
            sleep_ms(3000);
        }

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

