
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
#include "hardware/structs/vreg_and_chip_reset.h"

typedef enum {
    STATE_WELCOME, // index == 0,user select with lora or run in offline
    STATE_LORA_CONNECT, // system attempts to join lora
    STATE_MAIN_MENU, // user get period or go get pills directly
    STATE_SET_PERIOD, // if user wants to set their own period
    STATE_WAIT_CALIBRATE, // wait user to confirm at first initializing calibration
    STATE_CALIBRATE, // perform the calibration
    STATE_DISPENSING, // dispensing the pills one by one and lora reporting (if allow)
    STATE_FAULT_CHECK // try max 7 times to get enough pills, if not, show empty warning
} AppState_t;

AppState_t current_state = STATE_WELCOME;
uint32_t state_enter_time = 0;
int setting_period = DEFAULT_PERIOD;
// set the lora as default enable, backend system will try to connect lora anyway.
bool is_lora_enabled = true;
// these two variables to help statemachine marks
static bool has_sent_boot_message = false;
static bool is_recovery_mode = false;
// check if user press reset button
static bool is_reset_button_event = false;

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

// sleep that keeps lora alive
// the function make sure Lora would not block
void sleep_ms_with_lora(uint32_t ms) {
    uint32_t end_time = to_ms_since_boot(get_absolute_time()) + ms;
    while (to_ms_since_boot(get_absolute_time()) < end_time) {
        if (is_lora_enabled && lora_get_status() != LORA_STATUS_FAILED) {
            lora_get_ready_to_join();
        }
        led_blink_task();
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
    has_sent_boot_message = false;

    // check is the reboot by reset button, using the hardware register
    uint32_t reset_reason = vreg_and_chip_reset_hw->chip_reset;
    // check POR(power-on-reset) bit, if it is set, indicate the syetem reboot from power off
    if (reset_reason & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_POR_BITS) {
        is_reset_button_event = false;
        // must clear it for next so that we can detect it next time
        hw_clear_bits(&vreg_and_chip_reset_hw->chip_reset, VREG_AND_CHIP_RESET_CHIP_RESET_HAD_POR_BITS);
    } else {
        // POR is not set, power is always on, just press reset button
        is_reset_button_event = true;
    }

    // to check if reset button is pressed, does the system have an ongoing task.
    // the dispensed pills has not reach the target period
    bool is_task_incomplete = is_calibrated_dispenser() && (dispenser_get_dispensed_count()<dispenser_get_period());

    // turning power cut or reset button
    if (dispenser_was_motor_running_at_boot() || is_task_incomplete) {
        printf("[Boot] Power-off recovery detected. Skipping menu.\n");
        current_state = STATE_WAIT_CALIBRATE;
        // any incomplete task marks as true, no matter from power off or reset
        is_recovery_mode = true;
    } else {
        current_state = STATE_WELCOME;
        is_recovery_mode = false;
    }

    state_enter_time = to_ms_since_boot(get_absolute_time());
}

void statemachine_loop(void) {
    // try at the first no matter user choose or not
    if (is_lora_enabled) {
        if (lora_get_status() != LORA_STATUS_FAILED) {
            lora_get_ready_to_join();
        }
        // after joining lora, send Boot message first
        if (lora_get_status() == LORA_STATUS_JOINED && !has_sent_boot_message) {
            bool send_success;
            if (is_recovery_mode) {
                if (is_reset_button_event) {
                    send_success = lora_send_message("BOOT:RESET_RESUME");
                }else {
                    send_success = lora_send_message("BOOT:POWEROFF_DETECTED");
                }
            } else {
                send_success = lora_send_message("BOOT:NORMAL");
            }

            if (send_success) {
                printf("[Statemachine] Boot message sent.\n");
                has_sent_boot_message = true;
            }
        }
    }
    led_blink_task();

    int rot = get_encoder_rotation();
    bool is_encoder_pressed = is_encoder_button_pressed();
    uint32_t now = to_ms_since_boot(get_absolute_time());
    // only set the statemachine as invalid state index -1
    static AppState_t last_loop_state = -1;
    // very first time enter the system, this will be true
    bool is_state_changed = (current_state != last_loop_state);
    // update history for next loop
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
                sleep_ms(PAGE_TIMEOUT);
                change_state(STATE_MAIN_MENU);
            }

            else if (status == LORA_STATUS_FAILED) {
                oled_clear();
                oled_show_string(0, 2, "Join Failed!");
                oled_show_string(0, 4, "Go Offline Mode");

                is_lora_enabled = false;
                sleep_ms(PAGE_TIMEOUT);
                change_state(STATE_MAIN_MENU);
            }

            else if (now - state_enter_time > MAX_LORA_WAIT_TIMEOUT) {
                oled_clear();
                oled_show_string(0, 2, "Timeout!");
                oled_show_string(0, 4, "Go Offline Mode");

                is_lora_enabled = false;

                sleep_ms(PAGE_TIMEOUT);
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
            // only when user change the period, reload the oled, release CPU load
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
                oled_show_string(0, 6, "SW0- SW2+ Y->");

                char buf[16];
                sprintf(buf, "%d", setting_period);
                oled_show_string(0, 2, buf);
                last_drawn_period = setting_period;
            }

            if (is_encoder_pressed) {
                // pass the set period to other functions
                dispenser_set_period((uint8_t)setting_period);
                change_state(STATE_MAIN_MENU);
            }
        }
            break;

        case STATE_WAIT_CALIBRATE:
            if (is_state_changed) {
                led_set_mode(LED_BLINKING);
                if (is_calibrated_dispenser()) {
                    if (is_recovery_mode) {
                        oled_clear();
                        if (is_reset_button_event) {
                            oled_show_string(0, 0, "[ Reset ]");
                        }else {
                            oled_show_string(0, 0, "[ POWER LOSS ]");
                        }
                        oled_show_string(0, 2, "Auto-Recalib");
                        oled_show_string(0, 4, "in 10 seconds...");
                        oled_show_string(0, 6, "Keep Hands Away");

                        for (int i = POWER_ON_WARNING_TIME / 1000; i > 0; i--) {
                            char count_buf[16];
                            sprintf(count_buf, "in %d seconds...", i);
                            oled_show_string(0, 4, count_buf);

                            leds_set_brightness(BRIGHTNESS_ERROR_OCCUR);
                            for(int k=0; k< 1000 /(BlINK_INTERVAL_RECALIB*2); k++) {
                                led_set_mode(LED_ALL_ON);
                                sleep_ms_with_lora(BlINK_INTERVAL_RECALIB);
                                led_set_mode(LED_ALL_OFF);
                                sleep_ms_with_lora(BlINK_INTERVAL_RECALIB);
                            }
                        }

                        if (is_lora_enabled && !has_sent_boot_message) {
                            oled_clear();
                            oled_show_string(0, 0, "Wait Network...");
                            oled_show_string(0, 2, "Sending Status");

                            int wait_retries = MAX_JOIN_WAITING_TIME_MS / LORA_RETRY_INTERVAL_MS;
                            // we wait here buz if the user want lora,
                            // If we start the motor now, CPU usage might kill the LoRa connection.
                            while (wait_retries > 0) {
                                LoraStatus_t status = lora_get_status();

                                if (status == LORA_STATUS_JOINED) {
                                    oled_show_string(0, 4, "Joined!       ");
                                    sleep_ms_with_lora(1500);

                                    if (is_reset_button_event) {
                                        lora_send_message("BOOT:RESET_RESUME");
                                    } else {
                                        lora_send_message("BOOT:POWEROFF_DETECTED");
                                    }
                                    has_sent_boot_message = true;

                                    printf("[Statemachine] Boot message sent before recalib.\n");
                                    oled_show_string(0, 6, "Sent OK       ");
                                    sleep_ms_with_lora(PAGE_TIMEOUT);

                                    break;
                                }
                                else if (status == LORA_STATUS_FAILED) {
                                    oled_show_string(0, 4, "Join Failed   ");
                                    sleep_ms_with_lora(PAGE_TIMEOUT);
                                    break;
                                }

                                sleep_ms_with_lora(LORA_RETRY_INTERVAL_MS);
                                wait_retries--;
                            }
                        }

                        leds_set_brightness(BRIGHTNESS_NORMAL);
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
                if (!is_calibrated_dispenser()) {
                    oled_show_string(0, 4, "Calibrating...");
                    dispenser_calibration();
                } else {
                    if (is_recovery_mode) {
                        oled_show_string(0, 2, "Recovery");
                        oled_show_string(0, 4, is_reset_button_event ? "from Reset" : "from Power Off");
                    }else {
                        oled_show_string(0, 2, "Re-calibration");
                        oled_show_string(0, 4, "Need More Pills");
                    }
                    dispenser_recalibrate_from_poweroff();
                    dispenser_clear_boot_flag();
                    sleep_ms(PAGE_TIMEOUT);
                }

                // every time after calibration, no matter is initialization or recovery,
                // reset the encoder_pressed to false
                // otherwise when after recovery, the dispenser will start without users operation.
                is_encoder_button_pressed();

                if (is_calibrated_dispenser()) {
                    if (is_recovery_mode) {
                        sleep_ms(PAGE_TIMEOUT);
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
                // get it from eeprom
                int success_pill_count = dispenser_get_dispensed_count();
                int failure_pill_count = 0;
                // we set a separate variable considering the empty compartments occurs
                int total_pills_need = setting_period;

                sprintf(buf,"PILL: %d/%d",success_pill_count,setting_period);
                oled_show_string(0, 4, buf);
                // the task will finish only when the user get enough pills
                while (success_pill_count<total_pills_need) {
                    if (!is_calibrated_dispenser()) {
                        break;
                    }
                    bool result = do_dispense_single_round();
                    if (result) {
                        success_pill_count++;
                        sprintf(buf, "PILL: %d/%d", success_pill_count, setting_period);
                        oled_show_string(0, 4, buf);
                        // show a warning when the next days is the last day in the period
                        int dispensed = dispenser_get_dispensed_count();
                        if (dispensed == total_pills_need -1) {
                            oled_show_string(0, 6, "Need Refill");
                        }
                        // Doubt should give the pill first they enter or wait for one round first.
                        sleep_ms_with_lora(PILL_DISPENSE_INTERVAL);
                        oled_show_string(0, 6, "                ");

                    }else {
                        failure_pill_count++;
                        // allow 7 times retry
                        if (failure_pill_count>= MAX_DISPENSE_RETRIES) {
                            change_state(STATE_FAULT_CHECK);
                            return;
                        }
                        led_blinking_error(5,200);
                        // we do this buz when power off, application automatically recover LoRa connection
                        sleep_ms_with_lora(PILL_DISPENSE_INTERVAL);
                        oled_show_string(0, 6, "                ");
                    }
                }

                if (is_lora_enabled) {
                    sleep_ms_with_lora(PAGE_TIMEOUT);
                    lora_send_message("EMPTY");
                }

                oled_show_string(0, 4, "Finished!             ");
                is_recovery_mode = false;
                sleep_ms(PAGE_TIMEOUT);
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
                //dispenser_reset();
                if (is_lora_enabled) {
                    sleep_ms_with_lora(5000);
                    lora_send_message("STATUS:RESET");
                    sleep_ms_with_lora(5000);
                }
                // pretend it is recovery from reset, buz the period is not done yet
                is_recovery_mode = true;
                is_reset_button_event = true;
                change_state(STATE_CALIBRATE);
            }

            break;
    }
}
