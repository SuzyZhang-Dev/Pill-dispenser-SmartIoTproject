//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_STATEMACHINE_H
#define PILLDISPENSER_STATEMACHINE_H
#include "pico/types.h"

uint run_until_falling_edge(int direction);
void dispenser_calibration();
void dispenser_run_on_days(int days);


#endif //PILLDISPENSER_STATEMACHINE_H