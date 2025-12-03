#include "encoder.h"
#include "config.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"

static queue_t events;
static uint32_t last_press_time = 0;
// use volatile buz it could be changed in interrupt
static volatile bool is_encoder_pressed = false;

static void encoder_callback_handler(uint gpio, uint32_t events_mask) {
    if (gpio == ENCODER_A_GPIO) {
        int event_value = 0;
        if (gpio_get(ENCODER_B_GPIO)) {
            event_value = -1;
        } else {
            event_value = 1;
        }
        queue_try_add(&events, &event_value);
    }

    if (gpio == ENCODER_SW_GPIO) {
        uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
        if (current_time_ms - last_press_time > 200) { // debounce 200ms
            is_encoder_pressed = true;
            last_press_time = current_time_ms;
        }
    }

}
void encoder_init() {
    gpio_init(ENCODER_A_GPIO);gpio_set_dir(ENCODER_A_GPIO, GPIO_IN);gpio_disable_pulls(ENCODER_A_GPIO);
    gpio_init(ENCODER_B_GPIO);gpio_set_dir(ENCODER_B_GPIO, GPIO_IN);gpio_disable_pulls(ENCODER_B_GPIO);
    gpio_init(ENCODER_SW_GPIO);gpio_set_dir(ENCODER_SW_GPIO, GPIO_IN);gpio_pull_up(ENCODER_SW_GPIO);

    queue_init(&events, sizeof(int), 32);

    gpio_set_irq_enabled_with_callback(ENCODER_A_GPIO, GPIO_IRQ_EDGE_RISE, true, &encoder_callback_handler);
    gpio_set_irq_enabled(ENCODER_SW_GPIO, GPIO_IRQ_EDGE_FALL, true);
}

int get_encoder_rotation(void) {
    int received_value = 0;
    int value =0;
    while (queue_try_remove(&events, &received_value)) {
        value += received_value;
    }
    return value;
}

bool is_encoder_button_pressed(void) {
    if (is_encoder_pressed) {
        is_encoder_pressed = false;
        return true;
    }
    return false;
}
