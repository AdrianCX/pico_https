#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "tls_listener.h"
#include "http_session.h"

#include "wifi.h"

#define log printf

int main() {
    stdio_init_all();

    if (cyw43_arch_init()) {
        log("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        while (true) {
            printf("failed to connect to wifi, ssid: %s, password: %s\n", WIFI_SSID, WIFI_PASSWORD);
            sleep_ms(3000);
        }

        return 1;
    }

    log("Starting");
    
    TLSListener *client = new TLSListener();
    client->listen(443, HTTPSession::create);
    
    // Constantly print a message so we can see this in minicom over usb
    while (true) {
        printf("currently active\r\n");
        sleep_ms(5000);
    }

    cyw43_arch_deinit();
    return 0;
}

