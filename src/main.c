#include "config.h"
#include "dispenser.h"
#include <stdio.h>
#include <string.h>

#include "motor.h"
#include "sensor.h"
#include "pico/stdlib.h"


int main() {
    stdio_init_all();
    set_motor_pins();
    sensor_init();

    dispenser_calibration();

    while (1) {
        bool test = do_dispense_single_round(1);
        if (test) {
            printf("Pill dispensed successfully.\n");
        } else {
            printf("Pill dispensing failed.\n");
        }
    }
}