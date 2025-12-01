//
// Created by 张悦 on 1.12.2025.
//

#include "sensor.h"
#include "../config.h"
#include "hardware/gpio.h"

//GP27,28 as sensor input declared in config.h (all pins in config.h)
void sensor_init() {
    gpio_init(OPTO_SENSOR_PIN);
    gpio_set_dir(OPTO_SENSOR_PIN,GPIO_IN);
    gpio_pull_up(OPTO_SENSOR_PIN);
}

// Opto fork Reads zero when the opening is at the sensor.
int opto_fork_sensor_read() {
    return gpio_get(OPTO_SENSOR_PIN);
}