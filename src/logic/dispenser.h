//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_STATEMACHINE_H
#define PILLDISPENSER_STATEMACHINE_H
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

#define DEFAULT_DISPENSER_ROTATED_DIRECTION 1 //clock-wise
#define DISPENSER_BACK_DIRECTION (-1)

#define FAILURE_DISPENSE_PROMPT "Failure dispense."
#define CALIBRATION_ROUNDS 3 //temporary

#endif //PILLDISPENSER_STATEMACHINE_H