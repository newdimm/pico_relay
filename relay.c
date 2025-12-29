#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/binary_info.h"

#include "network.h"

#define RELAY_GPIO 15
#define PIR_GPIO 14
#define LIGHT_SENSOR_GPIO 26 // has to be ADC capable

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
    adc_gpio_init(LIGHT_SENSOR_GPIO);
    adc_select_input(0);

    gpio_init(RELAY_GPIO);
    gpio_set_dir(RELAY_GPIO, GPIO_OUT);
    // default state is OFF (active low)
    gpio_put(RELAY_GPIO, 1);

    gpio_init(PIR_GPIO);
    gpio_set_dir(PIR_GPIO, GPIO_IN);
    gpio_pull_down(PIR_GPIO);

#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
#endif

    bi_decl(bi_program_description("This is a relay example"));
    bi_decl(bi_1pin_with_name(PIR_GPIO, "PIR sensor polling"));
    bi_decl(bi_1pin_with_name(RELAY_GPIO, "Relay control"));
    bi_decl(bi_1pin_with_name(LIGHT_SENSOR_GPIO, "Light sensor measurement"));

    connect();

    uint relay_state = 0;

    while (1) {
        uint pir;
        uint day;

        if (!relay_state)
        {
            day = is_it_day();
            pir = gpio_get(PIR_GPIO);
            uint relay_on = !day && pir;

            if (relay_on)
            {
                gpio_put(RELAY_GPIO, 0);
                relay_state = 1;
                printf("turn ON");
            }
        }
        else { // relay is ON
            pir = gpio_get(PIR_GPIO);

            if (!pir)
            {
                gpio_put(RELAY_GPIO, 1);
                relay_state = 0;
                printf("turn OFF");
            }
        }

        printf("relay state: %d day: %d pir:%d\n", relay_state, day, pir);
        sleep_ms(1000);
    }
}

