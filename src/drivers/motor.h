//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_MOTOR_H
#define PILLDISPENSER_MOTOR_H

#define STEP_DELAY_MS 3

void set_motor_pins();
void motor_move_one_step(int direction);
void motor_stop();

#endif //PILLDISPENSER_MOTOR_H