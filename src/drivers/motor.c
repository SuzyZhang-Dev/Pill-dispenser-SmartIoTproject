//
// Created by 张悦 on 1.12.2025.
//

#include "motor.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "../config.h"

const bool half_step_sequence[8][4]={
    {1,0,0,0},
    {1,1,0,0},
    {0,1,0,0},
    {0,1,1,0},
    {0,0,1,0},
    {0,0,1,1},
    {0,0,0,1},
    {1,0,0,1}
};

// in module scope initialize motor pins, use it directly. and in logic layers no need to declare the instant again.
const uint motor_pins[4]= MOTOR_PINS;

void set_motor_pins() {
    for(int i=0;i<4;i++) {
        gpio_init(motor_pins[i]);
        gpio_set_dir(motor_pins[i],GPIO_OUT);
        gpio_put(motor_pins[i],0);
    }
}

void motor_move_one_step(int direction) {
    static int step_index = 0;
    step_index=(step_index+direction + 8) % 8;
    for(int i=0;i<4;i++) {
        gpio_put(motor_pins[i],half_step_sequence[step_index][i]);
    }
    sleep_ms(STEP_DELAY_MS);
}

void motor_stop() {
    for(int i=0;i<4;i++) {
        gpio_put(motor_pins[i],0);
    }
}