//
// Created by 张悦 on 1.12.2025.
//

#include "dispenser.h"
#include <stdio.h>

#include "../config.h"
#include <string.h>
#include "pico/stdlib.h"
#include "../drivers/motor.h"
#include "../drivers/sensor.h"
#include "../drivers/eeprom.h"

//default values for dispenser state
static bool is_calibrated = false;
static float step_per_revolution = 4096.0f;
static uint8_t pill_dispensed_count = 0;
static uint8_t pill_treatment_period = 7; // could be adjusted by ui, do it later.
static bool is_dispenser_empty() {
    return pill_dispensed_count >= pill_treatment_period;
} // emptiness check

void dispenser_init() {
    eeprom_init();
    DispenserState old_state;

    if (load_dispenser_state_from_eeprom(&old_state)) {
        is_calibrated = old_state.is_calibrated;
        step_per_revolution = old_state.step_per_revolution;
        pill_dispensed_count = old_state.pill_dispensed_count;
        pill_treatment_period = old_state.pill_treatment_period;
        printf("Welcome back. Loaded previous settings.\n");
        char log_message[MAX_MESSAGE_LENGTH];
        sprintf(log_message, "System Boot: Loaded previous settings. Calibrated:%d, Dispensed:%d/%d",
                is_calibrated, pill_dispensed_count, pill_treatment_period);
        log_write_message(log_message);

    }else {
        is_calibrated = false;
        step_per_revolution = 4096.0f;
        pill_dispensed_count = 0;
        pill_treatment_period = 7; // do it later flag
        printf("Welcome. No previous settings found.\n");
        log_write_message("System Boot: No previous settings found.");
    }

}

void dispenser_calibration() {
    float sum_steps = 0;
    int direction = DEFAULT_DISPENSER_ROTATED_DIRECTION; //clock-wise
    dispenser_align_with_opening(direction);
    for (int i = 0;i < CALIBRATION_ROUNDS; i++) {
        uint steps_this_rev = dispenser_align_with_opening(direction);
        sum_steps+=steps_this_rev;
    }

    //update global variables
    step_per_revolution = sum_steps / (float) CALIBRATION_ROUNDS;
    is_calibrated = true;
    pill_dispensed_count = 0;

    DispenserState calibrated_state;
    memset(&calibrated_state, 0, sizeof(DispenserState));

    calibrated_state.step_per_revolution = step_per_revolution;
    calibrated_state.is_calibrated = is_calibrated;
    calibrated_state.pill_dispensed_count = 0;
    calibrated_state.pill_treatment_period = 7; // do it later flag
    save_dispenser_state_to_eeprom(&calibrated_state);
    log_write_message("System Boot: Calibrated");
    printf("Calibrated. Ready to dispense.\n");
}

bool do_dispense_single_round() {
    if (!is_calibrated) return false;
    if (is_dispenser_empty()) {
        printf("Empty dispenser, need refill.\n");
        return false;
    } //is refill need another round calibration?

    //reset pill detected sensor, in case of false positive from last round
    sensor_reset_pill_detected();
    //rotate one part of the wheel at one time, could be designed by loop in main to control frequency.
    float steps_need = step_per_revolution / 8.0f;
    for (long i = 0; i< steps_need; i++) {
        motor_move_one_step(DEFAULT_DISPENSER_ROTATED_DIRECTION);
    }
    motor_stop();

    if (is_pill_dropped()) {
        // update global variables
        pill_dispensed_count++;
        printf("✅Dispensed %d/%d.\n", pill_dispensed_count, pill_treatment_period);

        char log_message[MAX_MESSAGE_LENGTH];
        sprintf(log_message,"Success: Dispensed %d/%d", pill_dispensed_count, pill_treatment_period);
        log_write_message(log_message);

        if (is_dispenser_empty()) {
            is_calibrated = false;
            pill_dispensed_count = 0;
            printf("⚠️Dispenser empty, please refill and recalibrate.\n");
            log_write_message("[log]dispenser empty, need refill and recalibration");
        }

        DispenserState current_state;
        memset(&current_state, 0, sizeof(DispenserState));
        current_state.step_per_revolution = step_per_revolution;
        current_state.is_calibrated = is_calibrated;
        current_state.pill_dispensed_count = pill_dispensed_count;
        current_state.pill_treatment_period = pill_treatment_period;

        save_dispenser_state_to_eeprom(&current_state);

        return true;
    } else {
        printf("❌Dispense failed.\n");
        log_write_message("[log]no pill detected");
        return false;
    }
}

//helper functions
uint dispenser_align_with_opening(int direction) {
    int steps_taken=0;
    int previous_state;
    int current_state = opto_fork_sensor_read();
    do {
        previous_state = current_state;
        motor_move_one_step(direction);
        steps_taken++;
        current_state=opto_fork_sensor_read();
    } while(!(previous_state == 1 && current_state == 0));

    return steps_taken;
}
bool is_pill_dropped() {
    uint64_t start_time = time_us_64() / 1000;
    while (time_us_64()/1000 - start_time < PILL_FALL_TIMEOUT_MS) {
        if (sensor_get_pill_detected() ) {
            return true;
        }
        sleep_ms(1);
    }
    return false;
}
bool is_calibrated_dispenser() {
    return is_calibrated;
} // what is this for?
