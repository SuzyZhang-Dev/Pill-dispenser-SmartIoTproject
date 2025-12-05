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

#define MAX_PERIOD 7
#define MAX_DOSE 3
#define DEFAULT_PERIOD 7
#define DEFAULT_DOSE 1
#define WELCOME_PAGE_TIMEOUT 5000

typedef enum {
    STATE_WELCOME,
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

int main() {
    system_init();
    //dispenser_reset();
    AppState_t last_loop_state = -1;

    while (true) {
        sleep_ms(20);

        if (lora_get_status() != LORA_STATUS_JOINED && lora_get_status() != LORA_FAILED) {
            lora_get_ready_to_join();
        }

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
                led_set_mode(LED_BLINKING);
                if (is_entry_frame) {
                    oled_show_string(0, 0, "Welcome to");
                    oled_show_string(0, 2, "DoseMate :)");
                    oled_show_string(0, 4, "Press to Start");
                    oled_show_string(90, 6, "-->");
                }

                if (is_encoder_pressed || now - last_input_time > WELCOME_PAGE_TIMEOUT) {
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
                    for (int i = 0; i < setting_period; i++) {
                        do_dispense_single_round();
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