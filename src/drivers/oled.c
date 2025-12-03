//
// Created by 张悦 on 3.12.2025.
//

#include "oled.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

void oled_init(void) {
    i2c_init(OLED_I2C_PORT, 400 * 1000); // 400kHz
    gpio_set_function(OLED_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_GPIO);
    gpio_pull_up(OLED_SCL_GPIO);
}

void oled_send_cmd(uint8_t cmd) {
    // 0x00 表示接下来的字节是命令
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(OLED_I2C_PORT, OLED_ADDR, buf, 2, false);
}

// 发送数据 (填充显存)
void oled_send_data(uint8_t data) {
    // 0x40 表示接下来的字节是数据
    uint8_t buf[2] = {0x40, data};
    i2c_write_blocking(OLED_I2C_PORT, OLED_ADDR, buf, 2, false);
}

// 极简初始化序列 (SSD1306 标准)
void oled_init_minimal() {
    oled_send_cmd(0xAE); // 关闭显示 (Display OFF)

    oled_send_cmd(0x20); // 设置内存寻址模式
    oled_send_cmd(0x00); // 00 = 水平寻址模式 (方便连续写入)

    oled_send_cmd(0x8D); // 电荷泵设置
    oled_send_cmd(0x14); // 开启电荷泵 (必须开启才能显示)

    oled_send_cmd(0xA1); // 段重映射 (根据屏幕方向可能需要改为 0xA0)
    oled_send_cmd(0xC8); // COM 扫描方向 (根据屏幕方向可能需要改为 0xC0)

    oled_send_cmd(0xAF); // 开启显示 (Display ON)
}

// 清屏
void oled_clear() {
    oled_send_cmd(0x21); // 设置列地址范围
    oled_send_cmd(0);
    oled_send_cmd(127);
    oled_send_cmd(0x22); // 设置页地址范围
    oled_send_cmd(0);
    oled_send_cmd(7);

    for (int i = 0; i < 128 * 8; i++) {
        oled_send_data(0x00);
    }
}