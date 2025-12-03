#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"
#include "drivers/led.h"
#include "drivers/encoder.h"
#include "drivers/lora.h"
#include "logic/dispenser.h" // 如果需要调用出药逻辑

int main() {
    // 1. 系统初始化
    stdio_init_all();
    sleep_ms(2000); // 缓冲时间，方便串口工具连接
    printf("=== Pill Dispenser System Boot ===\n");

    // 2. 驱动初始化
    led_init();
    encoder_init();
    lora_init();

    // 初始化出药逻辑(如果有)
    // dispenser_init();

    // 3. 初始状态：LED 闪烁表示正在启动/连接
    led_set_mode(LED_BLINKING);
    leds_set_brightness(BRIGHTNESS_NORMAL);

    bool has_sent_welcome = false; // 标记位，防止重复发送

    while (1) {
        // --- 核心任务 (必须在循环中不断调用) ---
        lora_get_ready_to_join();       // 处理 LoRa 状态机 (连接、接收)
        led_blink_task();  // 处理 LED 非阻塞闪烁

        // --- 业务逻辑 ---

        // 1. 获取 LoRa 状态并反馈到 UI
        LoraStatus_t net_state = lora_get_status();

        if (net_state == LORA_STATUS_JOINED) {
            // 如果连接成功，且还没发过欢迎消息
            if (!has_sent_welcome) {
                printf(">>> Main: Network Joined! Switching LED to Steady.\n");
                led_set_mode(LED_ALL_ON); // 连上了，灯常亮
                lora_send_message("Device Online"); // 发送一条上线通知
                has_sent_welcome = true;
            }
        }
        else if (net_state == LORA_FAILED) {
            // 如果连接失败，高亮报警
            led_set_mode(LED_ALL_ON);
            leds_set_brightness(BRIGHTNESS_MAX);
        }

        // 2. 处理按钮事件 (发送测试消息)
        if (is_encoder_button_pressed()) {
            if (net_state == LORA_STATUS_JOINED) {
                printf("[User] Button Pressed -> Sending 'Pill Dispensed'\n");
                lora_send_message("Pill Dispensed");

                // 可选：在这里调用出药马达逻辑
                // do_dispense_single_round();
            } else {
                printf("[User] Button Pressed -> Ignored (Not Connected)\n");
            }
        }

        // 3. 处理旋钮事件 (调节亮度)
        int rot = get_encoder_rotation();
        if (rot != 0) {
            // 这里仅仅演示，你可以把这个值加到 brightness 变量上
            printf("[User] Encoder Rotated: %d\n", rot);
        }

        sleep_ms(10); // 简单的 CPU 让渡
    }
}