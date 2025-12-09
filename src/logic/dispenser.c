#include "dispenser.h"
#include "statemachine.h"
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
static int max_recovery_step = 4096;
static uint8_t pill_dispensed_count = 0;
static uint8_t pill_treatment_period = 7;
static bool motor_running_at_boot = false;

//helper functions to change states in eeprom
static void state_from_globals(DispenserState *s, uint8_t motor_status) {
    memset(s, 0, sizeof(*s));
    s->step_per_revolution   = step_per_revolution;
    s->is_calibrated         = is_calibrated;
    s->pill_dispensed_count  = pill_dispensed_count;
    s->pill_treatment_period = pill_treatment_period;
    s->motor_status          = motor_status;
}
static void globals_from_state(const DispenserState *s) {
    is_calibrated        = s->is_calibrated;
    step_per_revolution  = s->step_per_revolution;
    pill_dispensed_count = s->pill_dispensed_count;
    pill_treatment_period= s->pill_treatment_period;
}

void move_to_falling_edge(int direction) {
    if (opto_fork_sensor_read() == 0) {
        while (opto_fork_sensor_read() == 0) {
            motor_move_one_step(direction);
            sleep_ms(1);
        }
    }

    while (opto_fork_sensor_read() == 1) {
        motor_move_one_step(direction);
        sleep_ms(1);
    }
}
static int measure_gap_width(int direction, int max_steps) {
    int gap_width = 0;
    while (opto_fork_sensor_read() == 0 && gap_width < max_steps) {
        motor_move_one_step(direction);
        gap_width++;
        sleep_ms(1);
    }
    return gap_width;
}

static void move_to_center_from_edge(int direction, int gap_width) {
    int steps_to_center = gap_width / 2;
    for (int i = 0; i < steps_to_center; i++) {
        motor_move_one_step(direction);
        sleep_ms(1);
    }
}

static bool is_dispenser_empty() {
    return pill_dispensed_count >= pill_treatment_period;
}
bool is_pill_dropped() {
    uint64_t start_time = time_us_64() / 1000;
    while (time_us_64() / 1000 - start_time < PILL_FALL_TIMEOUT_MS) {
        if (sensor_get_pill_detected()) {
            return true;
        }
        sleep_ms(1);
    }
    return false;
}
bool is_calibrated_dispenser() {
    return is_calibrated;
}

void dispenser_init() {
    eeprom_init();
    DispenserState old_state;

    if (load_dispenser_state_from_eeprom(&old_state)) {
        globals_from_state(&old_state);

        //printf("Welcome back. Loaded previous settings.\n");
        printf("Calibrated: %d, Dispensed: %d/%d, Motor status: %d\n",is_calibrated, pill_dispensed_count, pill_treatment_period,
               old_state.motor_status);

        char log_message[MAX_MESSAGE_LENGTH];
        sprintf(log_message, "BOOT:Calibrated:%d,Dispensed:%d/%d,MotorStatus:%d",is_calibrated, pill_dispensed_count, pill_treatment_period,
                old_state.motor_status);
        log_write_message(log_message);

        if (old_state.motor_status == 1) {
            motor_running_at_boot = true;
            lora_send_message("BOOT:POWEROFF_DETECTED");
        } else {
            motor_running_at_boot = false;
            lora_send_message("BOOT:NORMAL");
        }
    } else {
        motor_running_at_boot = false;
        is_calibrated = false;
        step_per_revolution = 4096.0f;
        pill_dispensed_count = 0;
        pill_treatment_period = 7;

        //printf("Welcome. No previous settings found.\n");
        log_write_message("System Boot: No previous settings found.");
        lora_send_message("BOOT:NEW");
    }
}

void dispenser_calibration() {
    int direction = DEFAULT_DISPENSER_ROTATED_DIRECTION;
    printf("Starting calibration...\n");

    move_to_falling_edge(direction);
    sleep_ms(200);

    uint measurements[CALIBRATION_ROUNDS];
    int last_gap_width = 0;

    for (int i = 0; i < CALIBRATION_ROUNDS; i++) {
        int gap_steps = 0;
        int blind_steps = 0;

        while (opto_fork_sensor_read() == 0) {
            motor_move_one_step(direction);
            gap_steps++;
            sleep_ms(1);
        }

        while (opto_fork_sensor_read() == 1) {
            motor_move_one_step(direction);
            blind_steps++;
            sleep_ms(1);
        }

        measurements[i] = gap_steps + blind_steps;
        last_gap_width = gap_steps;
        sleep_ms(100);
    }

    move_to_center_from_edge(direction, last_gap_width);
    motor_stop();

    float sum_steps = 0;
    for (int i = 0; i < CALIBRATION_ROUNDS; i++) {
        sum_steps += measurements[i];
    }
    step_per_revolution = sum_steps / (float)CALIBRATION_ROUNDS;

    is_calibrated = true;
    pill_dispensed_count = 0;

    DispenserState calibrated_state;
    state_from_globals(&calibrated_state,0);
    save_dispenser_state_to_eeprom(&calibrated_state);

    printf("Calibration Complete. Avg: %.2f steps/rev.",step_per_revolution);
}

bool do_dispense_single_round() {
    if (!is_calibrated) return false;

    DispenserState pre_state;
    state_from_globals(&pre_state, 1);
    save_dispenser_state_to_eeprom(&pre_state);

    sensor_reset_pill_detected();
    long steps_need = (long)(step_per_revolution / 8.0f + 0.5f);

    printf("[Debug] Starting to dispense round %d/%d...\n",
           pill_dispensed_count + 1, pill_treatment_period, steps_need);

    for (long i = 0; i < steps_need; i++) {
        motor_move_one_step(DEFAULT_DISPENSER_ROTATED_DIRECTION);
    }
    motor_stop();

    if (is_pill_dropped()) {
        pill_dispensed_count++;
        //printf("✅ Dispensed %d/%d.\n", pill_dispensed_count, pill_treatment_period);

        char log_message[MAX_MESSAGE_LENGTH];
        sprintf(log_message, "OK: %d/%d",pill_dispensed_count, pill_treatment_period);
        log_write_message(log_message);

        char lora_message[MAX_MESSAGE_LENGTH];
        snprintf(lora_message, MAX_MESSAGE_LENGTH, "OK:%d/%d",pill_dispensed_count, pill_treatment_period);
        lora_send_message(lora_message);

        if (is_dispenser_empty()) {
            is_calibrated = false;
            pill_dispensed_count = 0;
            //printf("⚠️ Dispenser empty, please refill and recalibrate.\n");
            log_write_message("EMPTY");

            sleep_ms_with_lora(6000);
            lora_send_message("EMPTY");
        }

        DispenserState success_state;
        state_from_globals(&success_state, 0);
        save_dispenser_state_to_eeprom(&success_state);

        return true;
    } else {
        //printf("❌ Dispense failed - no pill detected.\n");
        log_write_message("Dispense failed: no pill detected");
        lora_send_message("NOPILL");

        // if no pill fall the motor state should also be 0
        DispenserState fail_state;
        state_from_globals(&fail_state, 0);
        save_dispenser_state_to_eeprom(&fail_state);

        return false;
    }
}

void dispenser_recalibrate_from_poweroff() {
    printf("[Recovery] Starting power-off recovery...\n");
    // when do the recalibration, there should be at least one valid state in the eeprom.
    DispenserState old_state;
    if (!load_dispenser_state_from_eeprom(&old_state)) {
        printf("[Recovery] ERROR: No saved state found!\n");
        return;
    }
    step_per_revolution = old_state.step_per_revolution;
    pill_dispensed_count = old_state.pill_dispensed_count;
    pill_treatment_period = old_state.pill_treatment_period;
    printf("[Recovery] State loaded: dispensed=%d/%d, steps/rev=%.2f\n",pill_dispensed_count, pill_treatment_period, step_per_revolution);

    int target_slot = pill_dispensed_count; // how many pills already detected
    float steps_per_slot = step_per_revolution / 8.0f; // same as do the first time calibration
    int target_steps_from_home = (int)(target_slot * steps_per_slot);

    //printf("[Recovery] Target position: slot %d, %d steps from home\n",target_slot, target_steps_from_home);
    move_to_falling_edge(DISPENSER_BACK_DIRECTION);
    int gap_width = measure_gap_width(DISPENSER_BACK_DIRECTION, 100);
    move_to_center_from_edge(DEFAULT_DISPENSER_ROTATED_DIRECTION, gap_width);
    motor_stop();


    if (target_steps_from_home > 0) {
        for (int i = 0; i < target_steps_from_home; i++) {
            motor_move_one_step(DEFAULT_DISPENSER_ROTATED_DIRECTION);
        }
    } else {
        printf("[Recovery] Already at home position, no forward movement needed\n");
    }
    motor_stop();

    is_calibrated = true;
    old_state.motor_status = 0;
    save_dispenser_state_to_eeprom(&old_state);

    printf("[Recovery]Recovery complete! Ready to dispense slot %d\n",pill_dispensed_count + 1);
    log_write_message("Recovery from power-off successful");
}

void dispenser_reset() {
    //log_erase_all();
    DispenserState clean_state;
    state_from_globals(&clean_state, 0);
    save_dispenser_state_to_eeprom(&clean_state);

    is_calibrated = false;
    pill_dispensed_count = 0;

    log_write_message("System: Factory Reset Performed");
    printf("Factory Reset Complete. Please Restart.\n");
}

void dispenser_set_period(uint8_t period) {
    pill_treatment_period = period;
    DispenserState new_period_state;
    state_from_globals(&new_period_state, 0);
    save_dispenser_state_to_eeprom(&new_period_state);
    printf("[Debug] New period set %d.\n",period);
}

uint8_t dispenser_get_period() {
    return pill_treatment_period;
}
uint8_t dispenser_get_dispensed_count() {
    return pill_dispensed_count;
}

bool dispenser_was_motor_running_at_boot() {
    return motor_running_at_boot;
}
void dispenser_clear_boot_flag() {
    motor_running_at_boot = false;
}