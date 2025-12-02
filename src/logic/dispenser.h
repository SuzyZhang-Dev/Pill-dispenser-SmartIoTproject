//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_STATEMACHINE_H
#define PILLDISPENSER_STATEMACHINE_H
#include "pico/types.h"

uint dispenser_align_with_opening(int direction);
void dispenser_init();
void dispenser_calibration();
bool is_pill_dropped();
bool do_dispense_single_round();
bool is_calibrated_dispenser();

#define DEFAULT_DISPENSER_ROTATED_DIRECTION 1 //clock-wise
#define FAILURE_DISPENSE_PROMPT "Failure dispense."
#define CALIBRATION_ROUNDS 1 //temporary

#endif //PILLDISPENSER_STATEMACHINE_H