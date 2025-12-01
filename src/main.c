#include "config.h"
#include "dispenser.h"
#include <stdio.h>
#include <string.h>

#include "motor.h"
#include "sensor.h"
#include "pico/stdlib.h"


int main() {
    stdio_init_all();
    // Test motor functionality
    int n=8;
    set_motor_pins();
    printf("Motor System Initialized.\n");

    while (n==9) {
        int steps_to_move = 512;
        printf("Rotating...\n");
        for(int i = 0; i < steps_to_move; i++) {
            motor_move_one_step(1);
            sleep_ms(2);
        }
        printf("Done. Stopping.\n");
        motor_stop();
        sleep_ms(2000);
    }
}