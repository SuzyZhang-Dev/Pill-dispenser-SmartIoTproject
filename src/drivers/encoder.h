#ifndef PILLDISPENSER_ENCODER_H
#define PILLDISPENSER_ENCODER_H
#include <stdbool.h>
#include <stdint.h>
#include "pico/types.h"

void encoder_init();
int get_encoder_rotation(void);
bool is_encoder_button_pressed(void);
void encoder_gpio_handler(uint gpio, uint32_t events_mask);



#endif //PILLDISPENSER_ENCODER_H