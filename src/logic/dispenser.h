//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_DISPENSER_H
#define PILLDISPENSER_DISPENSER_H
#include "eeprom.h"
#include "pico/types.h"

uint dispenser_align_with_opening_centered(int direction);
void dispenser_init();
void dispenser_calibration();
bool is_pill_dropped();
bool do_dispense_single_round();
bool is_calibrated_dispenser();
void dispenser_recalibrate_from_poweroff();
void dispenser_reset();

void dispenser_set_period(uint8_t period);
uint8_t dispenser_get_period();
uint8_t dispenser_get_dispensed_count();
bool dispenser_was_motor_running_at_boot();
void dispenser_clear_boot_flag();


#define DEFAULT_DISPENSER_ROTATED_DIRECTION 1 //clock-wise
#define DISPENSER_BACK_DIRECTION (-1)

#define FAILURE_DISPENSE_PROMPT "Failure dispense."
#define CALIBRATION_ROUNDS 3 //temporary

#endif //PILLDISPENSER_DISPENSER_H