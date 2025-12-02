#include "eeprom.h"
#include <stdio.h>
#include "../config.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

//1.Private helpers
static void print_hex_dump(const char* label, uint8_t* data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}
static uint16_t crc16(const uint8_t *data_p, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF; // Initial value
    while (length--) {
        x = crc >>8 ^ *data_p++;
        x ^= x >>4;
        crc = (crc <<8) ^ (uint16_t)(x <<12) ^ (uint16_t)(x <<5) ^ (uint16_t)x;
    }
    return crc;
}
static void eeprom_write_bytes(uint16_t addr, uint8_t *data_p, size_t length) {
    uint8_t buf[2 + length];
    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    memcpy(&buf[2], data_p, length);
    i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buf, length + 2, false);
    sleep_ms(10);
}
static void eeprom_read_bytes(uint16_t addr, uint8_t *data_p, size_t length) {
    uint8_t addr_buf[2];
    addr_buf[0] = (uint8_t)(addr >> 8);
    addr_buf[1] = (uint8_t)(addr & 0xFF);
    i2c_write_blocking(I2C_PORT, EEPROM_ADDR, addr_buf, 2, true);
    i2c_read_blocking(I2C_PORT, EEPROM_ADDR, data_p, length, false);
}
static bool log_entry_is_valid(const uint8_t *buffer) {
    //The string must contain at least one character.
    if (buffer[0]==0) return false;

    //a terminating null character "\0"
    //define an unused number in the buffer entry.
    uint8_t null_position=-1;
    int i = 0;
    while (i<MAX_MESSAGE_LENGTH && buffer[i]!='\0') {
        i++;
    }
    if (i == MAX_MESSAGE_LENGTH) return false; //no null found in first 62 bytes
    null_position=i;

    int crc_position = null_position+1;
    if (crc_position+1>=LOG_ENTRY_SIZE) return false; //no space for crc

    // crc validation
    // string+'\0'+crc
    size_t total_length = null_position + 3;
    uint16_t crc16_result = crc16(buffer, total_length);
    if (crc16_result!=0) {
        return false;
    }
    return true;
}


void log_erase_all() {
    printf("Erasing all logs...\n");
    uint8_t zero_at_first_byte = 0; // setting first byte to zero marks the entry as invalid
    for (uint16_t i = 0; i < LOG_MAX_ENTRIES; i++) {
        uint16_t address = LOG_BASE_ADDRESS + i* LOG_ENTRY_SIZE;
        eeprom_write_bytes(address, &zero_at_first_byte, sizeof(zero_at_first_byte));
    }
    printf("All logs erased.\n");
}

void log_read_all() {
    printf("Reading all logs...\n");
    uint8_t buffer[LOG_ENTRY_SIZE];
    bool log_is_empty = true;
    for (uint16_t i=0; i<LOG_MAX_ENTRIES; i++) {
        uint16_t address = LOG_BASE_ADDRESS +(i* LOG_ENTRY_SIZE);
        eeprom_read_bytes(address, buffer, LOG_ENTRY_SIZE);


        if (log_entry_is_valid(buffer)) {
            log_is_empty = false;
            printf("Log Entry %d: %s\n", i, buffer);
        }
    }
    if (log_is_empty) {
        printf("No valid log entries found.\n");
    }
    printf("----------Reading Finished.-----------\n");
}

void log_write_message(const char *message) {
    int target_entry_index = -1;
    uint8_t buffer[LOG_ENTRY_SIZE];

    for (int i = 0;i < LOG_MAX_ENTRIES;i++) {
        uint16_t address = LOG_BASE_ADDRESS + i* LOG_ENTRY_SIZE;
        eeprom_read_bytes(address, &buffer[0], LOG_ENTRY_SIZE);
        if (!log_entry_is_valid(buffer)) {
            target_entry_index = i;
            break;
        }
    }
    // entry_index remains -1, means I go through all position for entries,and every one is valid.
    if (target_entry_index == -1) {
        printf("Log full, erasing all logs...\n");
        log_erase_all();
        target_entry_index = 0;
    }

    //use uint8_t because crc16 arguments need uint8_t
    uint8_t entry[LOG_ENTRY_SIZE]={0};
    size_t msg_length = strlen(message);
    if (msg_length > MAX_MESSAGE_LENGTH) {
        msg_length = MAX_MESSAGE_LENGTH;
    }
    memcpy(entry,message, msg_length);
    entry[msg_length] = '\0';

    uint16_t crc = crc16(entry, msg_length+1);
    entry[msg_length+1] = (uint8_t)(crc >> 8);
    entry[msg_length+2] = (uint8_t)(crc & 0xFF);
    uint16_t write_address = LOG_BASE_ADDRESS + (target_entry_index * LOG_ENTRY_SIZE);
    eeprom_write_bytes(write_address, entry, LOG_ENTRY_SIZE);
    printf("Log written at entry %d: %s\n", target_entry_index, message);
}

void eeprom_init() {
    i2c_init(I2C_PORT, 100 * 1000); //100kHz
    gpio_set_function(SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_GPIO);
    gpio_pull_up(SCL_GPIO);
}

void save_dispenser_state_to_eeprom(DispenserState *state) {
    size_t data_length = offsetof(DispenserState, crc16);
    state->crc16 = crc16((uint8_t *)state, data_length);
    //print_hex_dump("[DEBUG WRITE]", (uint8_t*)state, sizeof(DispenserState));
    eeprom_write_bytes(STORE_DISPENSER_ADDR, (uint8_t *)state, sizeof(DispenserState));
}

bool load_dispenser_state_from_eeprom(DispenserState *state) {
    eeprom_read_bytes(STORE_DISPENSER_ADDR, (uint8_t *)state, sizeof(DispenserState));
    //print_hex_dump("[DEBUG READ ]", (uint8_t*)state, sizeof(DispenserState));
    size_t data_length = offsetof(DispenserState, crc16);
    uint16_t computed_crc = crc16((uint8_t *)state, data_length);

    if (computed_crc != state->crc16) {
        printf("[EEPROM] CRC mismatch: stored=0x%04X, computed=0x%04X\n",
               state->crc16, computed_crc);
        return false;
    }

    printf("[EEPROM] State OK: step=%.2f, count=%d/%d, calibrated=%d\n",
           state->step_per_revolution,
           state->pill_dispensed_count,
           state->pill_treatment_period,
           state->is_calibrated);
    return true;
}