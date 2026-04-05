#include "network.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "lwip/dns.h"

#include "pico/cyw43_arch.h"

relay_network_t rnet;

#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_READY_TIMEOUT_MS 5000

void network_init(void)
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
        printf("network: init failed\n");
    }
    else {
        cyw43_arch_enable_sta_mode();

        rnet.init_done = true;
        printf("network: init\n");
    }
}

void network_periodic(uint32_t timeout_time_ms)
{
    cyw43_arch_poll();

    if (!rnet.init_done) {
        return;
    }

    if (!rnet.connected) {
        absolute_time_t current_time = get_absolute_time();

        if (!rnet.wifi_connect_time ||
                 absolute_time_diff_us(rnet.wifi_connect_time,
                                       current_time) > WIFI_CONNECT_TIMEOUT_MS * 1000)
        {
            rnet.wifi_connect_time = current_time;

            printf("network: connecting to SSID <%s>\n", WIFI_SSID);
            int res = cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
            if (res) {
                printf("network: wifi_connect_async failed with %d\n", res);
                return;
            }
            return;
        }

        int res = cyw43_wifi_link_status (&cyw43_state, CYW43_ITF_STA);

        switch (res) {
            case CYW43_LINK_JOIN:
                printf("network: connected\n");
                rnet.wifi_connect_time = 0;
                rnet.connected = true;
                rnet.ready = false;
                rnet.ready_wait_started = current_time;
                break;
            case CYW43_LINK_FAIL:
            case CYW43_LINK_NONET:
            case CYW43_LINK_BADAUTH:
                printf("network: connection failed with %d\n", res);
                rnet.wifi_connect_time = current_time;
                break;
            case CYW43_LINK_DOWN:
                // check again later
                break;
        }
        return;
    }

    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    if (status != CYW43_LINK_JOIN)
    {
        printf("network: disconnected %d\n", status);
        rnet.connected = false;
        rnet.wifi_connect_time = get_absolute_time();
        rnet.ready = false;
        return;
    }

    if (!rnet.ready)
    {
        absolute_time_t current_time = get_absolute_time();

        if (absolute_time_diff_us(rnet.ready_wait_started,
                                  current_time) > WIFI_READY_TIMEOUT_MS * 1000)
        {
            printf("network: ready\n");
            rnet.ready = true;

            ip_addr_t dns_addr;
            ipaddr_aton("8.8.8.8", &dns_addr);
            dns_setserver(0, &dns_addr);
        }
        return;
    }

    // connected, do periodic things
    cyw43_arch_wait_for_work_until(make_timeout_time_ms(timeout_time_ms));
}

