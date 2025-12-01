//
// Created by 张悦 on 1.12.2025.
//
#include "dispenser.h"
#include <stdio.h>

#include "../config.h"
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "../drivers/motor.h"
#include "../drivers/sensor.h"

bool is_calibrated=false;
float step_per_revolution=4096; //default value
//static volatile uint64_t last_interrupt_time_ms = 0;

// feature:
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

void dispenser_calibration() {
    printf("Welcome to Daily Medicine:). Start calibration...\n");
    float sum_steps=0;
    int direction = 1; //clock-wise

    dispenser_align_with_opening(direction);
    //printf("Start point found. Continuing calibration...\n");
    for (int i=0;i<3;i++) {
        uint steps_this_rev = dispenser_align_with_opening(direction);
        sum_steps+=steps_this_rev;
    }
    // update global variables
    // after done eeprom, store this global variable to eeprom.
    // so that don't need to calibrate again after power off.
    step_per_revolution = sum_steps / 3.0f;
    is_calibrated = true;
    printf("Calibrated. Steps per round is %.2f.\n",step_per_revolution);
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

bool do_dispense_single_round(int rounds) {
    if (!is_calibrated) return false;

    sensor_reset_pill_detected();
    float total_steps= rounds * step_per_revolution /8.0f;

    for (long i=0;i<total_steps;i++) {
        motor_move_one_step(1);
    }
    motor_stop();

    if (is_pill_dropped()) {
        return true;
    }
    return false;
}

