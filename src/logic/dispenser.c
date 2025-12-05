#include "dispenser.h"
#include <math.h>
#include <stdio.h>
#include "../config.h"
#include <string.h>
#include "lora.h"
#include "pico/stdlib.h"
#include "../drivers/motor.h"
#include "../drivers/sensor.h"
#include "../drivers/eeprom.h"

//default values for dispenser state
static bool is_calibrated = false;
static float step_per_revolution = 4096.0f;
static int max_recovery_step = 4096;
static uint8_t pill_dispensed_count = 0;
static uint8_t pill_treatment_period = 7;

static bool is_dispenser_empty() {
    return pill_dispensed_count >= pill_treatment_period;
}

void dispenser_recalibrate_from_poweroff() {
    printf("[Recovery] Starting power-off recovery...\n");

    DispenserState state;
    if (!load_dispenser_state_from_eeprom(&state)) {
        printf("[Recovery] ERROR: No saved state found!\n");
        return;
    }

    // 恢复状态变量
    step_per_revolution = state.step_per_revolution;
    pill_dispensed_count = state.pill_dispensed_count;
    pill_treatment_period = state.pill_treatment_period;

    printf("[Recovery] State loaded: dispensed=%d/%d, steps/rev=%.2f\n",
           pill_dispensed_count, pill_treatment_period, step_per_revolution);

    // 计算目标位置（已分药的格子数）
    int target_slot = pill_dispensed_count;  // 0-based: 0表示初始位置，1表示分了1个
    float steps_per_slot = step_per_revolution / 8.0f;
    int target_steps_from_home = (int)(target_slot * steps_per_slot + 0.5f);

    printf("[Recovery] Target position: slot %d, %d steps from home\n",
           target_slot, target_steps_from_home);

    // ===== 步骤1: 回退到校准孔（home位置）=====
    printf("[Recovery] Step 1: Moving back to calibration hole...\n");

    int back_step_count = 0;

    // 1a. 如果当前在孔内，先退出孔
    if (opto_fork_sensor_read() == 0) {
        printf("[Recovery] Currently in hole, moving out...\n");
        while (opto_fork_sensor_read() == 0 && back_step_count < max_recovery_step) {
            motor_move_one_step(DISPENSER_BACK_DIRECTION);
            back_step_count++;
        }
        printf("[Recovery] Moved out of hole: %d steps\n", back_step_count);
    }

    // 1b. 向后找第一个下降沿（找到校准孔）
    back_step_count = 0;
    printf("[Recovery] Searching for calibration hole...\n");
    while (opto_fork_sensor_read() == 1 && back_step_count < max_recovery_step) {
        motor_move_one_step(DISPENSER_BACK_DIRECTION);
        back_step_count++;
    }

    if (back_step_count >= max_recovery_step) {
        printf("[Recovery] ERROR: Could not find calibration hole!\n");
        return;
    }

    printf("[Recovery] Found calibration hole after %d steps\n", back_step_count);

    // 1c. 测量孔的宽度，移动到孔中心
    int gap_width = 0;
    while (opto_fork_sensor_read() == 0 && gap_width < 100) {
        motor_move_one_step(DISPENSER_BACK_DIRECTION);
        gap_width++;
    }

    printf("[Recovery] Gap width: %d steps\n", gap_width);

    // FIX START: 修复对齐逻辑并消除齿轮间隙
    // 此时我们已经向后（逆时针）穿过了整个孔，处于孔的另一侧（传感器读取为1，被遮挡）
    // 我们需要改为向前（顺时针）移动，直到再次检测到孔（传感器变0）
    // 这一步不仅能找到孔的确切边缘，还能消除电机换向时的齿轮间隙 (Backlash)

    printf("[Recovery] Switching direction to CW to find edge and clear backlash...\n");
    int align_steps = 0;
    // 向前走直到看见孔（传感器变0）
    while (opto_fork_sensor_read() == 1 && align_steps < 100) {
        motor_move_one_step(DEFAULT_DISPENSER_ROTATED_DIRECTION);
        align_steps++;
    }

    // 此时正好停在孔的边缘（刚进入孔）
    // 再向前走半个孔宽，即可到达中心
    int steps_to_center = gap_width / 2;
    printf("[Recovery] Found edge. Moving %d steps to center.\n", steps_to_center);

    for (int i = 0; i < steps_to_center; i++) {
        motor_move_one_step(DEFAULT_DISPENSER_ROTATED_DIRECTION);
    }

    motor_stop();
    printf("[Recovery] Centered at home position (calibration hole)\n");
    sleep_ms(500);

    // ===== 步骤2: 前进到目标位置 =====
    if (target_steps_from_home > 0) {
        printf("[Recovery] Step 2: Moving forward %d steps to slot %d...\n",
               target_steps_from_home, target_slot);

        for (int i = 0; i < target_steps_from_home; i++) {
            motor_move_one_step(DEFAULT_DISPENSER_ROTATED_DIRECTION);
            if (i % 100 == 0) {
                printf("[Recovery] Progress: %d/%d steps\n", i, target_steps_from_home);
            }
        }
    } else {
        printf("[Recovery] Already at home position, no forward movement needed\n");
    }

    motor_stop();

    // 更新状态
    is_calibrated = true;

    // 重置motor_status为稳定状态
    state.motor_status = 0;
    save_dispenser_state_to_eeprom(&state);

    printf("[Recovery] ✅ Recovery complete! Ready to dispense slot %d\n",
           pill_dispensed_count + 1);
    log_write_message("Recovery from power-off successful");
}

void dispenser_init() {
    eeprom_init();
    DispenserState old_state;

    if (load_dispenser_state_from_eeprom(&old_state)) {
        is_calibrated = old_state.is_calibrated;
        step_per_revolution = old_state.step_per_revolution;
        pill_dispensed_count = old_state.pill_dispensed_count;
        pill_treatment_period = old_state.pill_treatment_period;

        printf("Welcome back. Loaded previous settings.\n");
        printf("Calibrated: %d, Dispensed: %d/%d, Motor status: %d\n",
               is_calibrated, pill_dispensed_count, pill_treatment_period,
               old_state.motor_status);

        char log_message[MAX_MESSAGE_LENGTH];
        sprintf(log_message, "BOOT:Calibrated:%d,Dispensed:%d/%d,MotorStatus:%d",
                is_calibrated, pill_dispensed_count, pill_treatment_period,
                old_state.motor_status);
        log_write_message(log_message);

        if (old_state.motor_status == 1) {
            lora_send_message("BOOT:POWEROFF_DETECTED");
        } else {
            lora_send_message("BOOT:NORMAL");
        }
    } else {
        is_calibrated = false;
        step_per_revolution = 4096.0f;
        pill_dispensed_count = 0;
        pill_treatment_period = 7;

        printf("Welcome. No previous settings found.\n");
        log_write_message("System Boot: No previous settings found.");
        lora_send_message("BOOT:NEW");
    }
}

void move_to_falling_edge(int direction) {
    if (opto_fork_sensor_read() == 0) {
        while (opto_fork_sensor_read() == 0) {
            motor_move_one_step(direction);
            sleep_ms(1);
        }
    }

    while (opto_fork_sensor_read() == 1) {
        motor_move_one_step(direction);
        sleep_ms(1);
    }
}

void dispenser_calibration() {
    int direction = DEFAULT_DISPENSER_ROTATED_DIRECTION;
    printf("Starting calibration...\n");

    move_to_falling_edge(direction);
    sleep_ms(200);

    uint measurements[CALIBRATION_ROUNDS];
    int last_gap_width = 0;

    for (int i = 0; i < CALIBRATION_ROUNDS; i++) {
        int gap_steps = 0;
        int blind_steps = 0;

        while (opto_fork_sensor_read() == 0) {
            motor_move_one_step(direction);
            gap_steps++;
            sleep_ms(1);
        }

        while (opto_fork_sensor_read() == 1) {
            motor_move_one_step(direction);
            blind_steps++;
            sleep_ms(1);
        }

        measurements[i] = gap_steps + blind_steps;
        last_gap_width = gap_steps;
        sleep_ms(100);
    }

    int steps_to_center = last_gap_width / 2;
    printf("Aligning: Moving forward %d steps to center...\n", steps_to_center);

    for (int k = 0; k < steps_to_center; k++) {
        motor_move_one_step(direction);
        sleep_ms(1);
    }

    motor_stop();

    float sum_steps = 0;
    for (int i = 0; i < CALIBRATION_ROUNDS; i++) {
        sum_steps += measurements[i];
    }
    step_per_revolution = sum_steps / (float)CALIBRATION_ROUNDS;

    is_calibrated = true;
    pill_dispensed_count = 0;

    DispenserState calibrated_state;
    memset(&calibrated_state, 0, sizeof(calibrated_state));
    calibrated_state.step_per_revolution = step_per_revolution;
    calibrated_state.is_calibrated = true;
    calibrated_state.pill_dispensed_count = 0;
    calibrated_state.pill_treatment_period = pill_treatment_period;
    calibrated_state.motor_status = 0;
    save_dispenser_state_to_eeprom(&calibrated_state);

    printf("Calibration Complete. Avg: %.2f steps/rev. Position: Centered.\n",
           step_per_revolution);
}

bool do_dispense_single_round() {
    if (!is_calibrated) return false;
    if (is_dispenser_empty()) {
        printf("Empty dispenser, need refill.\n");
        return false;
    }

    // ⚠️ 重要：在开始转动前保存状态（标记motor_status=1）
    DispenserState pre_dispense_state;
    memset(&pre_dispense_state, 0, sizeof(DispenserState));
    pre_dispense_state.step_per_revolution = step_per_revolution;
    pre_dispense_state.is_calibrated = is_calibrated;
    pre_dispense_state.pill_dispensed_count = pill_dispensed_count;
    pre_dispense_state.pill_treatment_period = pill_treatment_period;
    pre_dispense_state.motor_status = 1;  // 标记：电机正在转动
    save_dispenser_state_to_eeprom(&pre_dispense_state);

    sensor_reset_pill_detected();
    long steps_need = (long)(step_per_revolution / 8.0f + 0.5f);

    printf("[Dispense] Starting to dispense round %d/%d (%ld steps)...\n",
           pill_dispensed_count + 1, pill_treatment_period, steps_need);

    for (long i = 0; i < steps_need; i++) {
        motor_move_one_step(DEFAULT_DISPENSER_ROTATED_DIRECTION);
    }
    motor_stop();

    if (is_pill_dropped()) {
        pill_dispensed_count++;
        printf("✅ Dispensed %d/%d.\n", pill_dispensed_count, pill_treatment_period);

        char log_message[MAX_MESSAGE_LENGTH];
        sprintf(log_message, "Success: Dispensed %d/%d",
                pill_dispensed_count, pill_treatment_period);
        log_write_message(log_message);

        char lora_message[MAX_MESSAGE_LENGTH];
        snprintf(lora_message, MAX_MESSAGE_LENGTH, "OK:%d/%d",
                 pill_dispensed_count, pill_treatment_period);
        lora_send_message(lora_message);

        if (is_dispenser_empty()) {
            is_calibrated = false;
            pill_dispensed_count = 0;
            printf("⚠️ Dispenser empty, please refill and recalibrate.\n");
            log_write_message("Dispenser empty, need refill");
            lora_send_message("EMPTY");
        }

        DispenserState success_state;
        memset(&success_state, 0, sizeof(DispenserState));
        success_state.step_per_revolution = step_per_revolution;
        success_state.is_calibrated = is_calibrated;
        success_state.pill_dispensed_count = pill_dispensed_count;
        success_state.pill_treatment_period = pill_treatment_period;
        success_state.motor_status = 0;  // 转动完成，标记为稳定
        save_dispenser_state_to_eeprom(&success_state);

        return true;
    } else {
        printf("❌ Dispense failed - no pill detected.\n");
        log_write_message("Dispense failed: no pill detected");
        lora_send_message("NOPILL");

        // 失败时，motor_status也要设为0（转动已完成，只是没检测到药）
        DispenserState fail_state;
        memset(&fail_state, 0, sizeof(DispenserState));
        fail_state.step_per_revolution = step_per_revolution;
        fail_state.is_calibrated = is_calibrated;
        fail_state.pill_dispensed_count = pill_dispensed_count;
        fail_state.pill_treatment_period = pill_treatment_period;
        fail_state.motor_status = 0;
        save_dispenser_state_to_eeprom(&fail_state);

        return false;
    }
}

bool is_pill_dropped() {
    uint64_t start_time = time_us_64() / 1000;
    while (time_us_64() / 1000 - start_time < PILL_FALL_TIMEOUT_MS) {
        if (sensor_get_pill_detected()) {
            return true;
        }
        sleep_ms(1);
    }
    return false;
}

bool is_calibrated_dispenser() {
    return is_calibrated;
}

void dispenser_reset() {
    log_erase_all();
    DispenserState clean_state;
    memset(&clean_state, 0, sizeof(DispenserState));

    clean_state.is_calibrated = false;
    clean_state.step_per_revolution = 4096.0f;
    clean_state.pill_dispensed_count = 0;
    clean_state.pill_treatment_period = 7;
    clean_state.motor_status = 0;
    save_dispenser_state_to_eeprom(&clean_state);

    is_calibrated = false;
    pill_dispensed_count = 0;

    log_write_message("System: Factory Reset Performed");
    printf("Factory Reset Complete. Please Restart.\n");
}