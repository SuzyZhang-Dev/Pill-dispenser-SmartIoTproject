//
// Created by 张悦 on 1.12.2025.
//

#include "sensor.h"
#include "../config.h"
#include "hardware/gpio.h"

static volatile bool pill_detected = false;

void piezo_irq_handler(uint gpio,uint32_t events) {
    if (gpio==PIEZO_SENSOR_PIN) {
        pill_detected = true;
    }
}

// Opto fork Reads zero when the opening is at the sensor.
int opto_fork_sensor_read() {
    return gpio_get(OPTO_SENSOR_PIN);
}

void sensor_init() {
    gpio_init(OPTO_SENSOR_PIN);
    gpio_set_dir(OPTO_SENSOR_PIN,GPIO_IN);
    gpio_pull_up(OPTO_SENSOR_PIN);

    gpio_init(PIEZO_SENSOR_PIN);
    gpio_set_dir(PIEZO_SENSOR_PIN,GPIO_IN);
    gpio_pull_up(PIEZO_SENSOR_PIN);

    gpio_set_irq_enabled_with_callback(
        PIEZO_SENSOR_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        &piezo_irq_handler);
}

void sensor_reset_pill_detected() {
    pill_detected = false;
}

bool sensor_get_pill_detected() {
    return pill_detected;
}



