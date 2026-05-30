#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/binary_info.h"

#include "network.h"
#include "mqtt.h"
#include "telegram.h"
#include "flash.h"

#define RELAY_GPIO 15
#define PIR_GPIO 14
#define HALL_GPIO 13
#define LIGHT_SENSOR_GPIO 26 // has to be ADC capable

#define dprintf(...) 

static int is_it_day(void)
{
    uint32_t threshold_mv = 35;

    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    uint32_t result_mv = floor(result * conversion_factor * 1000);
    dprintf("ADC value: 0x%03x, voltage: %d mV\n", result, result_mv);

    return result_mv > threshold_mv;
}

void log_event(char *event)
{
    printf("event: %s\n", event);
    mqtt_send("/log", event);
    telegram_send(event);
}

typedef struct 
{
    uint relay_state;
    uint door_state;
    uint is_night;
    uint motion_detected;

    absolute_time_t next_poll;
} relay_state_t;

relay_state_t relay_state;

#define RELAY_POLL_PERIOD_MS 1000

static void relay_do_poll(relay_state_t *state)
{
    state->motion_detected = gpio_get(PIR_GPIO);

    if (!state->relay_state) // light is OFF
    {
        // only measure ambient light when the light is off
        state->is_night = !is_it_day();
        uint light_needed = state->is_night && state->motion_detected;

        if (light_needed)
        {
            gpio_put(RELAY_GPIO, 0);
            state->relay_state = 1;
            log_event("light ON");
        }
    }
    else { // light is ON
        if (!state->motion_detected)
        {
            gpio_put(RELAY_GPIO, 1);
            state->relay_state = 0;
            log_event("light OFF");
        }
    }

    uint door = !gpio_get(HALL_GPIO);
    if (door != state->door_state)
    {
        state->door_state = door;

        if (state->door_state)
            log_event("door open");
        else
            log_event("door close");
    }
}

int main()
{
    stdio_init_all();
    adc_init();

    /* Ambient light sensor */
    adc_gpio_init(LIGHT_SENSOR_GPIO);
    adc_select_input(0);

    /* Light switch relay - turn the lighting on/off */
    gpio_init(RELAY_GPIO);
    gpio_set_dir(RELAY_GPIO, GPIO_OUT);
    // default state is OFF (active low)
    gpio_put(RELAY_GPIO, 1);

    /* PIR motion sensor */
    gpio_init(PIR_GPIO);
    gpio_set_dir(PIR_GPIO, GPIO_IN);
    gpio_pull_down(PIR_GPIO);

    /* Hall sensor garage door state */
    gpio_init(HALL_GPIO);
    gpio_set_dir(HALL_GPIO, GPIO_IN);

#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIhallN, 1);
#endif

    bi_decl(bi_program_description("Garden automation hub"));
    bi_decl(bi_1pin_with_name(PIR_GPIO, "PIR motion sensor"));
    bi_decl(bi_1pin_with_name(RELAY_GPIO, "Light switch relay"));
    bi_decl(bi_1pin_with_name(LIGHT_SENSOR_GPIO, "Ambient light sensor"));
    bi_decl(bi_1pin_with_name(HALL_GPIO, "Garage door state"));

    network_init();
    mqtt_init();
    telegram_init();
    flash_init();
    
    relay_state.next_poll = get_absolute_time();

    while (1) {

        if (absolute_time_diff_us(relay_state.next_poll, get_absolute_time()) > 0)
        {
            relay_do_poll(&relay_state);

            printf("light: %d night: %d motion: %d door: %d\n",
                relay_state.relay_state,
                relay_state.is_night,
                relay_state.motion_detected,
                relay_state.door_state);

            relay_state.next_poll = get_absolute_time() + RELAY_POLL_PERIOD_MS * 1000;
        }
        
        mqtt_periodic();
        telegram_periodic();
        network_periodic(1000);
    }
}

