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
float step_per_revolution=4096;

// feature: motor and sensor control
uint run_until_falling_edge(int direction) {
    int steps_taken=0;
    int previous_state;
    int current_state=opto_fork_sensor_read();
    do {
        previous_state=current_state;
        motor_move_one_step(direction);
        steps_taken++;
        current_state=opto_fork_sensor_read();
    } while(!(previous_state == 1 && current_state == 0));

    return steps_taken;
}

void dispenser_calibration() {
    printf("Starting calibration\n");
    uint step_count_per_revolution[3]; //count 3 laps and do average
    float sum_steps=0;
    int direction = 1; //clock-wise

    run_until_falling_edge(direction);
    printf("Start point found. Continuing calibration...\n");

    for (int i=0;i<3;i++) {
        printf("Starting round %d\n",i+1);
        uint steps_this_rev=run_until_falling_edge(direction);
        step_count_per_revolution[i]=steps_this_rev;
        sum_steps+=steps_this_rev;
        printf("Round %d completed, steps: %d.\n",i+1,steps_this_rev);
    }

    step_per_revolution=sum_steps/3.0f;
    is_calibrated = true;
    printf("Calibrated\n");
    printf("Steps of each round: %d,%d,%d.\n",step_count_per_revolution[0],
        step_count_per_revolution[1],step_count_per_revolution[2]);
    printf("Steps per round is %.2f.\n",step_per_revolution);
}

void dispenser_run_on_days(int days) {
    if (!is_calibrated) {
        printf("No calibration found\n");
        return;
    }

    float total_steps=days/8.0f * step_per_revolution;

    for (long i=0;i<total_steps;i++) {
        motor_move_one_step(1);
    }

    set_motor_pins((bool[4]){0,0,0,0});
    printf("[%d]Days Pills Completed.\n",days);
}