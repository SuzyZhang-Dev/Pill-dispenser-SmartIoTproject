#include "encoder&button.h"
#include "config.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/util/queue.h"

#define BUTTON_DEBOUNCE_MS 200

static queue_t events;
static uint32_t last_press_time = 0;
// use volatile buz it could be changed in interrupt
static volatile bool is_encoder_pressed = false;

static bool button_debounced(uint gpio, uint32_t *last_press_time) {
    bool is_pressed = false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (is_pressed && now - *last_press_time > BUTTON_DEBOUNCE_MS) {
        *last_press_time = now;
        return true;
    }
    return false;
}


void encoder_gpio_handler(uint gpio, uint32_t events_mask) {
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
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_press_time > BUTTON_DEBOUNCE_MS){
            is_encoder_pressed = true;
            last_press_time = now;
        }
    }
}

void encoder_init() {
    gpio_init(ENCODER_A_GPIO);
    gpio_set_dir(ENCODER_A_GPIO, GPIO_IN);
    gpio_disable_pulls(ENCODER_A_GPIO);
    gpio_init(ENCODER_B_GPIO);
    gpio_set_dir(ENCODER_B_GPIO, GPIO_IN);
    gpio_disable_pulls(ENCODER_B_GPIO);
    gpio_init(ENCODER_SW_GPIO);
    gpio_set_dir(ENCODER_SW_GPIO, GPIO_IN);
    gpio_pull_up(ENCODER_SW_GPIO);

    queue_init(&events, sizeof(int), 32);

    gpio_set_irq_enabled(ENCODER_A_GPIO, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(ENCODER_SW_GPIO, GPIO_IRQ_EDGE_FALL, true);
}

int get_encoder_rotation(void) {
    int received_value = 0;
    int value = 0;
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


void buttons_init() {
    gpio_init(SW0_GPIO);
    gpio_set_dir(SW0_GPIO, GPIO_IN);
    gpio_pull_up(SW0_GPIO);
    gpio_init(SW1_GPIO);
    gpio_set_dir(SW1_GPIO, GPIO_IN);
    gpio_pull_up(SW1_GPIO);
    gpio_init(SW2_GPIO);
    gpio_set_dir(SW2_GPIO, GPIO_IN);
    gpio_pull_up(SW2_GPIO);
}

static uint32_t last_sw0_time = 0;
//static uint32_t last_sw1_time = 0;
static uint32_t last_sw2_time = 0;

bool is_sw0_pressed(void) {
    return button_debounced(SW0_GPIO, &last_sw0_time);
}

bool is_sw2_pressed(void) {
    return button_debounced(SW2_GPIO, &last_sw2_time);
}
