//
// Created by 张悦 on 3.12.2025.
//

#include "oled.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define MAX_PAGE 8

void oled_init(void) {
    i2c_init(OLED_I2C_PORT, 400 * 1000); // 400kHz
    gpio_set_function(OLED_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_GPIO);
    gpio_pull_up(OLED_SCL_GPIO);
}

void oled_send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(OLED_I2C_PORT, OLED_ADDR, buf, 2, false);
}

void oled_send_data(uint8_t data) {
    uint8_t buf[2] = {0x40, data};
    i2c_write_blocking(OLED_I2C_PORT, OLED_ADDR, buf, 2, false);
}

// SSD1306 datasheet P37 command table
// https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
void oled_init_minimal() {
    oled_send_cmd(0xAE); // Power off at the beginning
    oled_send_cmd(0x20); // Memory addressing mode
    oled_send_cmd(0x00); // Horizontal addressing mode (auto-wrap to next line)
    oled_send_cmd(0x8D); // Enable internal charge pump
    oled_send_cmd(0x14); // generate voltage from 3.3v to 7v around
    oled_send_cmd(0xA1); // mirror display
    oled_send_cmd(0xC8); // mirror display vertically
    oled_send_cmd(0xAF);// Power on
}


void oled_clear() {
    oled_send_cmd(0x21); // set column address
    oled_send_cmd(0); // from column 0
    oled_send_cmd(127); // to colum 127
    oled_send_cmd(0x22); // set page address
    oled_send_cmd(0); //from page 0 (8 pixels)
    oled_send_cmd(7); //to page 7

    for (int i = 0; i < 128 * 8; i++) {
        oled_send_data(0x00);
    }
}

//ssd1306 128*64 oled; x [0,127], y [0-7]; every 8 pixel per page;
void oled_set_position(uint8_t x, uint8_t y) {
    oled_send_cmd(0xb0 +y);
    oled_send_cmd((x & 0xf0) >>4 | 0x10);
    oled_send_cmd(x & 0x0f);
}

void oled_show_char(uint8_t x, uint8_t y, char chr) {
    uint8_t c = 0;
    uint8_t i = 0;
    c = chr - ' ';
    if (x>120) {
        x=0;
        y++;
    }

    oled_set_position(x, y);
    for (i=0;i<MAX_PAGE;i++) {
        oled_send_data(F8X16[c * 16 + i]);
    }
    oled_set_position(x, y+1);
    for (i=0;i<MAX_PAGE;i++) {
        oled_send_data(F8X16[c * 16 + i+8]);
    }
}

void oled_show_string(uint8_t x, uint8_t y, const char *str) {
    while (*str!='\0') {
        oled_show_char(x, y, *str);
        x+=8;
        str++;
    }
}