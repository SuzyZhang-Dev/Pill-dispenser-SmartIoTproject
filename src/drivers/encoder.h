//
// Created by 张悦 on 3.12.2025.
//

#ifndef PILLDISPENSER_ENCODER_H
#define PILLDISPENSER_ENCODER_H
#include <stdbool.h>

void encoder_init();
int get_encoder_rotation(void);
bool is_encoder_button_pressed(void);


#endif //PILLDISPENSER_ENCODER_H