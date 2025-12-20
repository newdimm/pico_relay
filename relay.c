#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/binary_info.h"

#include "network.h"

// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#include "pico/cyw43_arch.h"

#define RELAY_GPIO 15
#define PIR_GPIO 14

volatile int relay_on = 0;

int is_it_day(void)
{
    uint32_t threshold_mv = 35;

    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    uint32_t result_mv = floor(result * conversion_factor * 1000);
    printf("ADC value: 0x%03x, voltage: %d mV\n", result, result_mv);

    return result_mv > threshold_mv;
}

int main()
{
    stdio_init_all();
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    int rc = cyw43_arch_init();
    hard_assert(rc == PICO_OK);

    gpio_init(RELAY_GPIO);
    gpio_set_dir(RELAY_GPIO, GPIO_OUT);

    gpio_init(PIR_GPIO);
    gpio_set_dir(PIR_GPIO, GPIO_IN);
    gpio_pull_down(PIR_GPIO);

    bi_decl(bi_program_description("This is a relay example"));
    bi_decl(bi_1pin_with_name(CYW43_WL_GPIO_LED_PIN, "On-board LED"));

    connect();

    while (1) {
        uint day = is_it_day();
        uint pir = gpio_get(PIR_GPIO);
        uint relay_on = !day && pir;

        if (relay_on) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            gpio_put(RELAY_GPIO, 0);
        }
        else {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            gpio_put(RELAY_GPIO, 1);
        }
        sleep_ms(250);

        printf("relay state: %d day: %d pir:%d\n", relay_on, day, pir);
        sleep_ms(1000);
    }
}

