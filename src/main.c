#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "eeprom.h"
#include "motor.h"
#include "sensor.h"
#include "dispenser.h"


char get_user_command() {
    int ch = getchar_timeout_us(100000);
    if (ch != PICO_ERROR_TIMEOUT) {
        return (char)ch;
    }
    return 0;
}

int main() {
    stdio_init_all();
    sleep_ms(3000);

    set_motor_pins();
    sensor_init();
    dispenser_init();

    while (1) {
        char cmd = get_user_command();
        if (cmd == 'i') {
            if (is_calibrated_dispenser()) {
                printf("Status: Calibrated. Ready.\n");
            } else {
                printf("Status: NOT Calibrated.\n");
            }
        }

        if (cmd =='c') {
            dispenser_calibration() ;
        }

        if (cmd=='s') {
            bool test = do_dispense_single_round();
            if (test) {
                printf("Day2 dispensed successfully.\n");
            } else {
                printf("Day2 dispensing failed.\n");
            }
        }
        if (cmd=='r') {
            log_read_all();
        }

        if (cmd=='e') {
            log_erase_all();
        }
        sleep_ms(10);
    }
}


