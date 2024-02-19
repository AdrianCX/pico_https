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

    uint32_t start = to_us_since_boot(get_absolute_time())/1000;
    while (true) {
        cyw43_arch_poll();

        uint32_t now = to_us_since_boot(get_absolute_time())/1000;
        if (now - start > 5000)
        {
            start = now;
            trace("currently active: %s\r\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
        }
    }

    cyw43_arch_deinit();
    return 0;
}

