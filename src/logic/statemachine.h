//
// Created by 张悦 on 7.12.2025.
//

#ifndef PILLDISPENSER_STATEMACHINE_H
#define PILLDISPENSER_STATEMACHINE_H
#include "pico/types.h"

void statemachine_init(void);
void statemachine_loop(void);
void statemachine_gpio_callback(uint gpio, uint32_t events);
void sleep_ms_with_lora(uint32_t ms);
#endif //PILLDISPENSER_STATEMACHINE_H
