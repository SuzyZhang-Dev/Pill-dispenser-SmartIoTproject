//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_STATEMACHINE_H
#define PILLDISPENSER_STATEMACHINE_H
#include "pico/types.h"

uint dispenser_align_with_opening(int direction);
void dispenser_calibration();
bool is_pill_dropped();
bool do_dispense_single_round(int rounds) ;

#endif //PILLDISPENSER_STATEMACHINE_H