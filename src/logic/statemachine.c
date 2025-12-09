
#include "statemachine.h"
#include <stdio.h>
#include "hardware/watchdog.h"
#include "config.h"
#include "pico/stdlib.h"
#include "oled.h"
#include "sensor.h"
#include "led.h"
#include "encoder&button.h"
#include "lora.h"
#include "dispenser.h"

typedef enum {
    STATE_WELCOME, // index == 0
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
int setting_period = DEFAULT_PERIOD;
bool is_lora_enabled = true;

//if we use encoder, there is a conflict using encoder interrupt with callback.
//set a whole callback handler for main logic
void statemachine_gpio_callback(uint gpio, uint32_t events) {
    encoder_gpio_handler(gpio, events);
    piezo_irq_handler(gpio, events);
}


static void change_state(AppState_t new_state) {
    current_state = new_state;
    state_enter_time = to_ms_since_boot(get_absolute_time());
    oled_clear();
}

void sleep_ms_with_lora(uint32_t ms) {
    uint32_t end_time = to_ms_since_boot(get_absolute_time()) + ms;
    while (to_ms_since_boot(get_absolute_time()) < end_time) {
        if (is_lora_enabled && lora_get_status() != LORA_STATUS_FAILED) {
            lora_get_ready_to_join();
        }
        led_blink_task();
        //watchdog_update();
        sleep_ms(10);
    }
}

void statemachine_init(void) {
    current_state = STATE_WELCOME;
    state_enter_time = 0;
    setting_period = DEFAULT_PERIOD;
    // by defauly, state will try to connect to LoRa automatically.
    // if user choose not to use it, it ends the procedure.
    // that makes users wait for shorter time.
    is_lora_enabled = true;

    if (dispenser_was_motor_running_at_boot()) {
        printf("[Boot] Power-off recovery detected. Skipping menu.\n");
        current_state = STATE_WAIT_CALIBRATE;
    } else {
        current_state = STATE_WELCOME;
    }

    state_enter_time = to_ms_since_boot(get_absolute_time());
}

void statemachine_loop(void) {
    if (is_lora_enabled) {
        if (lora_get_status() != LORA_STATUS_FAILED) {
            lora_get_ready_to_join();
        }
    }
    led_blink_task();

    int rot = get_encoder_rotation();
    bool is_encoder_pressed = is_encoder_button_pressed();
    uint32_t now = to_ms_since_boot(get_absolute_time());
    // only set the statemachine as -1 at the very first time enter the system
    static AppState_t last_loop_state = -1;
    bool is_state_changed = (current_state != last_loop_state);
    last_loop_state = current_state;

    switch (current_state) {
        case STATE_WELCOME:
            static int mode_index = 0; // 0=With LoRa, 1=Offline
            bool need_refresh = true;
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
                    is_lora_enabled = true;
                    change_state(STATE_LORA_CONNECT);
                } else {
                    is_lora_enabled = false;
                    printf("[User] Selected Offline Mode.\n");
                    change_state(STATE_MAIN_MENU);
                }
            }
            break;

        case STATE_LORA_CONNECT:
            if (is_state_changed) {
                oled_show_string(0, 0, "[ Connecting ]");
                oled_show_string(0, 3, "Joining LoRaWAN");
                oled_show_string(0, 5, "Please Wait...");
                led_set_mode(LED_BLINKING);
            }

            LoraStatus_t status = lora_get_status();
            if (status == LORA_STATUS_JOINED) {
                oled_clear();
                oled_show_string(0, 2, "Success!");
                oled_show_string(0, 4, "LoRa Online");
                led_set_mode(LED_ALL_ON);
                sleep_ms(WELCOME_PAGE_TIMEOUT);
                change_state(STATE_MAIN_MENU);
            }

            else if (status == LORA_STATUS_FAILED) {
                oled_clear();
                oled_show_string(0, 2, "Join Failed!");
                oled_show_string(0, 4, "Go Offline Mode");

                is_lora_enabled = false;
                sleep_ms(WELCOME_PAGE_TIMEOUT);
                change_state(STATE_MAIN_MENU);
            }

            else if (now - state_enter_time > 20000) {
                oled_clear();
                oled_show_string(0, 2, "Timeout!");
                oled_show_string(0, 4, "Go Offline Mode");

                is_lora_enabled = false;

                sleep_ms(2000);
                change_state(STATE_MAIN_MENU);
            }

            if (is_encoder_pressed) {
                printf("[User] Cancelled LoRa joining.\n");
                is_lora_enabled = false;
                change_state(STATE_MAIN_MENU);
            }
            break;

        case STATE_MAIN_MENU:
        {
            static int menu_index = 0;

            if (rot != 0) {
                menu_index += rot;
                if (menu_index > 1) menu_index = 0;
                else if (menu_index < 0) menu_index = 1;
            }

            oled_show_string(10, 0, "Main menu");
            oled_show_string(10, 2, menu_index == 0 ? "> Get Pills " : "  Get Pills   ");
            oled_show_string(10, 4, menu_index == 1 ? "> Set Dose  " : "  Set Dose    ");


            if (is_encoder_pressed) {
                if (menu_index == 1) change_state(STATE_SET_PERIOD);
                if (menu_index == 0) change_state(STATE_WAIT_CALIBRATE);
            }
        }
            break;

        case STATE_SET_PERIOD:
        {
            static int last_drawn_period = -1;

            if (is_state_changed) {
                last_drawn_period = -1;
                setting_period = dispenser_get_period();
            }

            bool is_period_changed = false;
            if (is_sw2_pressed()) { setting_period++; is_period_changed = true; }
            if (is_sw0_pressed()) { setting_period--; is_period_changed = true; }

            if (is_period_changed) {
                if (setting_period > MAX_PERIOD) setting_period = MAX_PERIOD;
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
                dispenser_set_period((uint8_t)setting_period);
                change_state(STATE_MAIN_MENU);
            }
        }
            break;

        case STATE_WAIT_CALIBRATE:
            if (is_state_changed) {
                led_set_mode(LED_BLINKING);
                if (is_calibrated_dispenser()) {
                    // recovery from power off, or pill dispensed is less than setting period
                    if (dispenser_was_motor_running_at_boot()) {
                        oled_clear();
                        oled_show_string(0, 0, "[ POWER LOSS ]");
                        oled_show_string(0, 2, "Auto-Recalib");
                        oled_show_string(0, 4, "in 5 seconds...");
                        oled_show_string(0, 6, "Keep Hands Away");


                        for (int i = POWER_ON_WARNING_TIME/1000; i > 0; i--) {
                            char count_buf[16];
                            sprintf(count_buf, "in %d seconds...", i);
                            oled_show_string(0, 4, count_buf);
                            led_blinking_error(5,100);
                        }
                        change_state(STATE_CALIBRATE);
                        break;
                    }
                    else {
                        oled_show_string(0, 0, "System Resume");
                        oled_show_string(0, 2, "Press to Align");
                    }
                }

                else {
                    oled_show_string(0, 0, "Init Calibrate");
                    oled_show_string(0, 2, "Press to Start");
                }
            }

            if (is_encoder_pressed) {
                change_state(STATE_CALIBRATE);
            }
            break;

        case STATE_CALIBRATE:
            if (is_state_changed) {
                led_set_mode(LED_BLINKING);
                bool need_manual_start = false;
                if (!is_calibrated_dispenser()) {
                    oled_show_string(0, 4, "Calibrating...");
                    dispenser_calibration();
                } else {
                    if (dispenser_was_motor_running_at_boot()) {
                        oled_show_string(0, 2, "Recovery");
                        oled_show_string(0, 4, "from Power Off");
                        need_manual_start = true;
                    }else {
                        oled_show_string(0, 2, "Re-calibration");
                        oled_show_string(0, 4, "Need More Pills");
                        need_manual_start = false;
                    }
                    dispenser_recalibrate_from_poweroff();
                    dispenser_clear_boot_flag();
                    sleep_ms(1000);
                }

                // every time after calibration, no matter is initialization or recovery,
                // reset the encoder_pressed to false
                // otherwise when after recovery, the dispenser will start without users operation.
                is_encoder_button_pressed();

                if (is_calibrated_dispenser()) {
                    if (need_manual_start) {
                        sleep_ms(1000);
                        change_state(STATE_DISPENSING);
                    }else {
                        oled_clear();
                        oled_show_string(0, 2, "Done!");
                        oled_show_string(0, 4, "Press to Run");
                        led_set_mode(LED_ALL_ON);
                    }
                }
            }
            // wait for users next movement to dispense
            if (is_calibrated_dispenser() && is_encoder_pressed) {
                change_state(STATE_DISPENSING);
            }
            break;

        case STATE_DISPENSING:
            led_set_mode(LED_ALL_OFF);
            if (is_state_changed) {
                oled_show_string(0, 0, "Dispensing...");
                setting_period = dispenser_get_period();
                char buf[16];
                int success_pill_count = dispenser_get_dispensed_count();
                int failure_pill_count = 0;
                int total_pills_need = setting_period;

                sprintf(buf,"PILL: %d/%d",success_pill_count,setting_period);
                oled_show_string(0, 4, buf);
                for (int i = success_pill_count; i < setting_period; i++) {
                    bool result = do_dispense_single_round();
                    if (!is_calibrated_dispenser()) {
                        break;
                    }
                    if (result) {
                        success_pill_count++;
                        sprintf(buf, "PILL: %d/%d", success_pill_count, setting_period);
                        oled_show_string(0, 4, buf);

                        int dispensed = dispenser_get_dispensed_count();
                        if (dispensed == total_pills_need -1) {
                            oled_show_string(0, 6, "Need Refill");
                        }
                        // Doubt should give the pill first they enter or wait for one round first.
                        sleep_ms_with_lora(PILL_DISPENSE_INTERVAL);
                        oled_show_string(0, 6, "                ");

                    }else {
                        failure_pill_count++;
                        if (failure_pill_count>= MAX_DISPENSE_RETRIES) {
                            change_state(STATE_FAULT_CHECK);
                            return;
                        }
                        led_blinking_error(5,200);
                        // we do this buz when power off, application automatically recover LoRa connection
                        sleep_ms_with_lora(PILL_DISPENSE_INTERVAL);
                    }
                }
                oled_show_string(0, 4, "Finished!             ");
                sleep_ms(2000);
                change_state(STATE_MAIN_MENU);
            }
            break;

        case STATE_FAULT_CHECK:
            if (is_state_changed) {
                oled_clear();
                oled_show_string(0, 0, "[ EMPTY! ]");
                oled_show_string(0, 2, "Dispenser Empty");
                oled_show_string(0, 4, "Please Refill");
                oled_show_string(0, 6, "Press to Reset");

                if (is_lora_enabled) {
                    sleep_ms_with_lora(5000);
                    lora_send_message("ALARM:EMPTY");
                    sleep_ms_with_lora(5000);

                }
                led_set_mode(LED_BLINKING);
                leds_set_brightness(BRIGHTNESS_ERROR_OCCUR);
            }

            if (is_encoder_pressed) {
                leds_set_brightness(BRIGHTNESS_NORMAL);
                led_set_mode(LED_ALL_OFF);
                dispenser_reset();
                if (is_lora_enabled) {
                    sleep_ms_with_lora(3000);
                    lora_send_message("STATUS:RESET");
                    sleep_ms_with_lora(3000);
                }
                change_state(STATE_MAIN_MENU);
            }

            break;
    }
}
