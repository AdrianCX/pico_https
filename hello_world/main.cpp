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


// Pico W like header
extern "C" {
#include "pico_eth/ethpio_arch.h"
}

// Demo tests headers (taken from pico-examples)

void network_init(void)
{
    // Wrap the network configuration in a function to free the memory occupied by it at the end of the call
    ethpio_parameters_t config;

    config.pioNum = 0; // Pio, 0 or 1 (should be 0 for now, 1 untested)

    config.rx_pin = 18; // RX pin (RX+ : positive RX ethernet pair side)

    // The two pins of the TX must follow each other ! TX- is ALWAYS first, TX+ next
    config.tx_neg_pin = 16; // TX pin (TX- : negative TX ethernet pair side, 17 will be TX+)

    // Network MAC address (6 bytes) - PLEASE CHANGE IT !
    // Recover MAC and Ethernet transformers from network cards (or motherboards) that you throw away to give them a second life on your Pico-E !
    MAC_ADDR(config.mac_address, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05);

    // Network default values (When DHCP disabled or unavailable)
    IP4_ADDR(&config.default_ip_v4, 192, 168, 1, 110);      // Network IPv4
    IP4_ADDR(&config.default_netmask_v4, 255, 255, 255, 0); // Net mask
    IP4_ADDR(&config.default_gateway_v4, 192, 168, 1, 1);   // Gateway

    // Network host name, you'll probably need some sort of DNS server to see it
    // Not to be confused with Netbios name, Netbios not activated but available (pico_lwip_netbios lib, see Pico SDK)
    strcpy(config.hostname, "lwIP_Pico");

    config.enable_dhcp_client = true; // Enable DHCP client

    eth_pio_arch_init(&config); // Apply, ARP & DHCP take time to set up : network will not be available immediatly
    // You could use dhcp_supplied_address(netif) (see lwIP docs) to determine if you're online
}

int main() {
    // Board init
    set_sys_clock_khz(120000, true);

    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_disable_sta_mode();
    cyw43_arch_disable_ap_mode();

    /*int err_code = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);

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
    */

    network_init();
    
    start_logging_server();
    
    trace("Starting");
    
    TLSListener *client = new TLSListener();
    client->listen(443, RequestHandler::create);

    // Constantly print a message so we can see this in minicom over usb
    while (true) {
        eth_pio_arch_poll();
    }

    cyw43_arch_deinit();
    return 0;
}

