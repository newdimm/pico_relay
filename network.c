#include "network.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "password.h"

#ifdef HAVE_WIRELESS

#include "pico/cyw43_arch.h"

int connect(void)
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_UK)) {
        printf("network: init failed\n");
        return 1;
    }

    printf("network: init\n");

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("network: failed to connected\n");
        return 1;
    }

    printf("network: connected\n");

    return 0;
}

#else

int connect(void)
{
    printf("network: not supported\n");
}

#endif
