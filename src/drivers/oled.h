//
// Created by 张悦 on 3.12.2025.
//

#ifndef PILLDISPENSER_OLED_H
#define PILLDISPENSER_OLED_H
#include <stdint.h>

void oled_init(void);
void oled_send_cmd(uint8_t cmd);
void oled_send_data(uint8_t data);
void oled_init_minimal(void);
void oled_clear(void);

#endif //PILLDISPENSER_OLED_H