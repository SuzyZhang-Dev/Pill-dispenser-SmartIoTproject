//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_SENSOR_H
#define PILLDISPENSER_SENSOR_H
#include <stdbool.h>

#include "pico/types.h"

void sensor_init();
void sensor_reset_pill_detected();
bool sensor_get_pill_detected();
int opto_fork_sensor_read();
void piezo_irq_handler(uint gpio,uint32_t events);
#endif //PILLDISPENSER_SENSOR_H