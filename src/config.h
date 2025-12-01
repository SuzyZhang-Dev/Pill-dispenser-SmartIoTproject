//
// Created by 张悦 on 30.11.2025.
//

#ifndef LAB5EX2_CONFIG_H
#define LAB5EX2_CONFIG_H

#define I2C_PORT i2c0
#define SDA_GPIO 16
#define SCL_GPIO 17
#define EEPROM_ADDR 0x50 //because A0,A1 are grounded
#define MAX_EEPROM_ADDR (32*1024)

#define SW0_GPIO 9 //control LED0
#define SW1_GPIO 8 //control LED1
#define SW2_GPIO 7 //control LED2

#define LED0 22
#define LED1 21
#define LED2 20

#define LED_BIT_0 (1 << 0)  // 0b00000001
#define LED_BIT_1 (1 << 1)  // 0b00000010 = 2
#define LED_BIT_2 (1 << 2) // 0b0000100 = 4
#define DEFAULT_STATE LED_BIT_1 // 0b00000010 = 2

typedef struct ledstate {
    uint8_t state;
    uint8_t not_state;
} ledstate;
// EEPROM has 32768 bytes to store and ledstate takes 2 bytes.
// the last index is 32767,
// so the highest address we can use to store ledstate is 32766.
#define STORE_LEDSTATE_ADDR (MAX_EEPROM_ADDR-sizeof(ledstate))

#define LOG_BASE_ADDRESS 0
#define LOG_SIZE 2048 //bytes
#define LOG_ENTRY_SIZE 64 //bytes
#define LOG_MAX_ENTRIES (LOG_SIZE / LOG_ENTRY_SIZE) //32 entries
#define INPUT_BUFFER_SIZE 64 //bytes
#define MAX_MESSAGE_LENGTH 61


#endif //LAB5EX2_CONFIG_H