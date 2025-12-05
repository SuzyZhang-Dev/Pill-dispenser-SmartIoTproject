#include <stdio.h>
#include "hardware/watchdog.h"
#include "config.h"
#include "pico/stdlib.h"
#include "motor.h"
#include "oled.h"
#include "sensor.h"
#include "led.h"
#include "encoder.h"
#include "lora.h"
#include "dispenser.h"



typedef enum {
    STATE_WELCOME,
    STATE_LORA_CONNECT,
    STATE_MAIN_MENU,
    STATE_SET_PERIOD,
    STATE_WAIT_CALIBRATE,
    STATE_CALIBRATE,
    STATE_DISPENSING,
    STATE_FAULT_CHECK
} AppState_t;

AppState_t current_state = STATE_WELCOME;
uint32_t state_enter_time = 0;
uint32_t last_input_time = 0;
int setting_period = DEFAULT_PERIOD;
bool is_lora_enabled = true;

//if we use encoder, there is a conflict using encoder interrupt with callback.
//set a whole callback handler for main logic
void main_gpio_callback(uint gpio, uint32_t events) {
    encoder_gpio_handler(gpio, events);
    piezo_irq_handler(gpio, events);
}

static void system_init() {
    stdio_init_all();
    sleep_ms(2000);
    gpio_set_irq_callback(&main_gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
    led_init();
    buttons_init();
    encoder_init();
    set_motor_pins();
    sensor_init();
    dispenser_init();
    lora_init();
    oled_init();
    oled_init_minimal();
    oled_clear();
    printf("[User] System Init.\n");
}

void change_state(AppState_t new_state) {
    current_state = new_state;
    state_enter_time = to_ms_since_boot(get_absolute_time());
    last_input_time = state_enter_time;
    oled_clear();
}

void sleep_ms_with_lora(uint32_t ms) {
    uint32_t end_time = to_ms_since_boot(get_absolute_time()) + ms;
    while (to_ms_since_boot(get_absolute_time()) < end_time) {
        // 只要 LoRa 开启且没坏，就一直轮询消息
        if (is_lora_enabled && lora_get_status() != LORA_STATUS_FAILED) {
            lora_get_ready_to_join();
        }
        // 保持 LED 闪烁任务 (非阻塞部分)
        led_blink_task();

        sleep_ms(10); // 短暂休眠释放 CPU
    }
}

int main() {
    system_init();
    //dispenser_reset();
    AppState_t last_loop_state = -1;
    is_lora_enabled = true;

    while (true) {
        sleep_ms(20);


        if (is_lora_enabled) {
            if (lora_get_status() != LORA_STATUS_FAILED) {
                lora_get_ready_to_join();
            }
        }
        led_blink_task();

        int rot = get_encoder_rotation();
        bool is_encoder_pressed = is_encoder_button_pressed();
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (rot != 0 || is_encoder_pressed) {
            last_input_time = now;
        }

        bool is_entry_frame = (current_state != last_loop_state);
        last_loop_state = current_state;

        switch (current_state) {
            case STATE_WELCOME:
                static int mode_index = 0; // 0=With LoRa, 1=Offline
                bool need_refresh = is_entry_frame;

                // 允许 LED 闪烁提示系统在运行
                led_set_mode(LED_BLINKING);

                if (rot != 0) {
                    mode_index += rot;
                    if (mode_index > 1) mode_index = 0;
                    else if (mode_index < 0) mode_index = 1;
                    need_refresh = true;
                }

                if (need_refresh) {
                    oled_show_string(0, 0, "--- DoseMate ---");
                    oled_show_string(0, 2, "Select Mode:");
                    if (mode_index == 0) {
                        oled_show_string(0, 4, "> With LoRa   ");
                        oled_show_string(0, 6, "  Offline Mode");
                    } else {
                        oled_show_string(0, 4, "  With LoRa   ");
                        oled_show_string(0, 6, "> Offline Mode");
                    }
                }

                if (is_encoder_pressed) {
                    if (mode_index == 0) {
                        // 用户选择 LoRa
                        is_lora_enabled = true;
                        change_state(STATE_LORA_CONNECT); // 进入连接等待界面
                    } else {
                        // 用户选择离线
                        is_lora_enabled = false;
                        printf("[User] Selected Offline Mode.\n");
                        change_state(STATE_MAIN_MENU); // 直接进主菜单
                    }
                }
                break;

            case STATE_LORA_CONNECT:
                if (is_entry_frame) {
                    oled_clear();
                    // [FIX] 补全显示代码，修复黑屏问题
                    oled_show_string(0, 0, "[ Connecting ]");
                    oled_show_string(0, 3, "Joining LoRaWAN");
                    oled_show_string(0, 5, "Please Wait...");
                    oled_show_string(0, 7, "(Press -> Skip)");

                    led_set_mode(LED_BLINKING);
                }

                // 检查状态
                LoraStatus_t status = lora_get_status();

                // 2.1 成功
                if (status == LORA_STATUS_JOINED) {
                    oled_clear();
                    oled_show_string(0, 2, "Success!");
                    oled_show_string(0, 4, "LoRa Online");
                    led_set_mode(LED_ALL_ON); // 常亮表示成功
                    sleep_ms(3000);
                    change_state(STATE_MAIN_MENU);
                }
                //2.2 失败 (LoRa 模块报告 Failed)
                else if (status == LORA_STATUS_FAILED) {
                    oled_clear();
                    oled_show_string(0, 2, "Join Failed!");
                    oled_show_string(0, 4, "Go Offline Mode");

                    is_lora_enabled = false; // 既然坏了，就自动禁用，防止后台干扰
                    sleep_ms(2000);
                    change_state(STATE_MAIN_MENU);
                }
                // // 超时处理 (例如等待超过 20秒还没连上)
                else if (now - state_enter_time > 20000) {
                    oled_clear();
                    oled_show_string(0, 2, "Timeout!");
                    oled_show_string(0, 4, "Go Offline Mode");

                    is_lora_enabled = false; // 既然用户等得不耐烦了，通常建议切离线

                    sleep_ms(2000);
                    change_state(STATE_MAIN_MENU);
                }

                // 允许用户手动跳过
                if (is_encoder_pressed) {
                    printf("[User] Cancelled LoRa joining.\n");
                    is_lora_enabled = false; // 用户主动跳过，不再尝试
                    change_state(STATE_MAIN_MENU);
                }
                break;

            case STATE_MAIN_MENU:
            {
                static int menu_index = 0;
                bool need_refresh = is_entry_frame;

                if (rot != 0) {
                    menu_index += rot;
                    if (menu_index > 1) menu_index = 0;
                    else if (menu_index < 0) menu_index = 1;
                    need_refresh = true;
                }

                if (need_refresh) {
                    oled_show_string(10, 0, "Main menu");
                    oled_show_string(10, 2, menu_index == 0 ? "> Set Dose  " : "  Set Dose    ");
                    oled_show_string(10, 4, menu_index == 1 ? "> Get Pills " : "  Get Pills   ");
                }

                if (is_encoder_pressed) {
                    if (menu_index == 0) change_state(STATE_SET_PERIOD);
                    if (menu_index == 1) change_state(STATE_WAIT_CALIBRATE);
                }
            }
            break;

            case STATE_SET_PERIOD:
            {
                static int last_drawn_period = -1;
                if (is_entry_frame) last_drawn_period = -1;

                bool is_period_changed = false;
                if (is_sw2_pressed()) { setting_period++; is_period_changed = true; }
                if (is_sw0_pressed()) { setting_period--; is_period_changed = true; }

                if (is_period_changed) {
                    if (setting_period > DEFAULT_PERIOD) setting_period = MAX_PERIOD;
                    if (setting_period < 1) setting_period = 1;
                }

                if (setting_period != last_drawn_period) {
                    oled_clear();
                    oled_show_string(0, 0, "Set Period");
                    oled_show_string(0, 4, "Max 7 days");
                    oled_show_string(0, 6, "SW0- SW2+ E<-");

                    char buf[16];
                    sprintf(buf, "%d", setting_period);
                    oled_show_string(0, 2, buf);
                    last_drawn_period = setting_period;
                }

                if (is_encoder_pressed) {
                    change_state(STATE_MAIN_MENU);
                }
            }
            break;

            case STATE_WAIT_CALIBRATE:
                if (is_entry_frame) {
                    if (is_calibrated_dispenser()) {
                        oled_show_string(0, 0, "Resume Calibrate");
                    }else {
                        oled_show_string(0, 0, "Init Calibrate");
                    }
                    oled_show_string(0, 2, "Press to Start");
                }
                if (is_encoder_pressed) {
                    change_state(STATE_CALIBRATE);
                }
                break;

            case STATE_CALIBRATE:
                if (is_entry_frame) {
                    if (!is_calibrated_dispenser()) {
                        oled_show_string(0, 4, "Calibrating...");
                        dispenser_calibration();
                    }else {
                        oled_show_string(0, 4, "Recovery from Poweroff");
                        dispenser_recalibrate_from_poweroff();
                        sleep_ms(1000);
                    }

                    is_encoder_button_pressed();

                    if (is_calibrated_dispenser()) {
                        oled_clear();
                        oled_show_string(0, 2, "Done!");
                        oled_show_string(0, 4, "Press to Run");
                        led_set_mode(LED_ALL_ON);
                    }
                }

                if (is_calibrated_dispenser() && is_encoder_pressed) {
                    change_state(STATE_DISPENSING);
                }
                break;

            case STATE_DISPENSING:
                if (is_entry_frame) {
                    oled_show_string(0, 0, "Dispensing...");
                    int success_pill_count =0;
                    for (int i = 0; i < setting_period; i++) {
                        bool result = do_dispense_single_round();
                        if (!is_calibrated_dispenser()) {
                            break;
                        }
                        if (result) {
                            success_pill_count++;
                            sleep_ms(PILL_DISPENSE_INTERVAL);
                        }else {
                            led_blinking_nonblocking(5,200);
                            sleep_ms_with_lora(PILL_DISPENSE_INTERVAL);
                        }
                    }
                    oled_show_string(0, 2, "Finished!");
                    sleep_ms(2000);
                    change_state(STATE_MAIN_MENU);
                }
                break;

            case STATE_FAULT_CHECK:
                break;
        }
    }
}