#include "dispenser.h"
#include <stdio.h>

#include "../config.h"
#include <string.h>

#include "lora.h"
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
        lora_send_message("BOOT:RECOVER");
    }else {
        is_calibrated = false;
        step_per_revolution = 4096.0f;
        pill_dispensed_count = 0;
        pill_treatment_period = 7; // do it later flag
        printf("Welcome. No previous settings found.\n");
        log_write_message("System Boot: No previous settings found.");
        lora_send_message("BOOT:NEW");
    }

}

void move_to_falling_edge(int direction) {
    // buz we need calibration every time, even it looks already aligned, move out for new calibration.
    if (opto_fork_sensor_read() == 0) {
        // printf("Sensor inside gap, moving out...\n");
        while (opto_fork_sensor_read() == 0) {
            motor_move_one_step(direction);
            sleep_ms(1);
        }
    }
    // when sensor read from 1 to 0, stop.
    while (opto_fork_sensor_read() == 1) {
        motor_move_one_step(direction);
        sleep_ms(1);
    }
}

void dispenser_calibration() {
    int direction = DEFAULT_DISPENSER_ROTATED_DIRECTION; // clockwise
    printf("Starting calibration...\n");

    //find first falling 0 point as "home"
    move_to_falling_edge(direction);
    sleep_ms(200);

    uint measurements[CALIBRATION_ROUNDS];
    // record gap between perfectly align to the openside
    int last_gap_width = 0;

    for (int i = 0; i < CALIBRATION_ROUNDS; i++) {
        int gap_steps = 0;   // when sensor == 0
        int blind_steps = 0; // 遮挡部分的步数 (Sensor == 1)

        // we already stop at sensor == 0 before enter the loop, and find when 0 to 1.
        // that is how far we "over" the perfectly aligned
        while (opto_fork_sensor_read() == 0) {
            motor_move_one_step(direction);
            gap_steps++;
            sleep_ms(1);
        }
        // sensor is 0 when enter this loop, for record blind_step is until next 1 to 0.
        while (opto_fork_sensor_read() == 1) {
            motor_move_one_step(direction);
            blind_steps++;
            sleep_ms(1);
        }

        measurements[i] = gap_steps + blind_steps;
        // for power off align, each time will have different gap with the align.
        last_gap_width = gap_steps;
        //printf("Round %d: Total %d steps (Gap: %d, Blind: %d)\n", i + 1, measurements[i], gap_steps, blind_steps);
        sleep_ms(100);  // mark for me
    }

    int steps_to_center = last_gap_width / 2;
    printf("Aligning: Moving forward %d steps to center...\n", steps_to_center);

    for (int k = 0; k < steps_to_center; k++) {
        motor_move_one_step(direction);
        sleep_ms(1);
    }

    motor_stop();

    float sum_steps = 0;
    for (int i = 0; i < CALIBRATION_ROUNDS; i++) {
        sum_steps += measurements[i];
    }
    step_per_revolution = sum_steps / (float)CALIBRATION_ROUNDS;

    // update global variables
    is_calibrated = true;
    pill_dispensed_count = 0;

    DispenserState calibrated_state;
    memset(&calibrated_state, 0, sizeof(calibrated_state));
    calibrated_state.step_per_revolution = step_per_revolution;
    calibrated_state.is_calibrated = true;
    save_dispenser_state_to_eeprom(&calibrated_state);

    printf("Calibration Complete. Avg: %.2f steps/rev. Position: Centered.\n", step_per_revolution);
}

bool do_dispense_single_round() {
    if (!is_calibrated) return false;
    if (is_dispenser_empty()) {
        printf("Empty dispenser, need refill.\n");
        return false;
    }
    //reset pill detected sensor, in case of false positive from last round
    sensor_reset_pill_detected();
    //rotate one part of the wheel at one time, could be designed by loop in main to control frequency.
    long steps_need = (long)(step_per_revolution / 8.0f + 0.5f);

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

        char lora_message[MAX_MESSAGE_LENGTH];
        snprintf(lora_message, MAX_MESSAGE_LENGTH,"OK:%d/%d",pill_dispensed_count, pill_treatment_period);
        lora_send_message(lora_message);

        if (is_dispenser_empty()) {
            is_calibrated = false;
            pill_dispensed_count = 0;
            printf("⚠️Dispenser empty, please refill and recalibrate.\n");
            log_write_message("[log]dispenser empty, need refill and recalibration");
            lora_send_message("EMPTY");
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
        lora_send_message("NOPILLCOME");
        return false;
    }
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
}
