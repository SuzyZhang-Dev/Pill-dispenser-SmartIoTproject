//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_EEPROM_H
#define PILLDISPENSER_EEPROM_H
#include <stdint.h>
#include "hardware/i2c.h"
#include <string.h>
#include <stdbool.h>

#define EEPROM_ADDR 0x50 //because A0,A1 are grounded
#define MAX_EEPROM_ADDR (32*1024) //32768 bytes

#define STORE_DISPENSER_ADDR (MAX_EEPROM_ADDR - 64)
#define LOG_BASE_ADDRESS 0
#define LOG_SIZE 4096 //bytes
#define LOG_ENTRY_SIZE 64 //bytes
#define LOG_MAX_ENTRIES (LOG_SIZE / LOG_ENTRY_SIZE) //64 entries
//#define INPUT_BUFFER_SIZE 64 //bytes
#define MAX_MESSAGE_LENGTH 61

typedef struct {
    float step_per_revolution; //32 bits
    uint8_t pill_treatment_period; // 1-7
    uint8_t pill_dispensed_count; // 0-6
    bool is_calibrated;
    uint16_t crc16;
} DispenserState; //total around 16 bytes, keep 64 bytes for these structure


void log_erase_all();
void log_read_all();
void log_write_message(const char *message);

void eeprom_init();
void save_dispenser_state_to_eeprom(DispenserState *state);
bool load_dispenser_state_from_eeprom(DispenserState *state);


#endif //PILLDISPENSER_EEPROM_H