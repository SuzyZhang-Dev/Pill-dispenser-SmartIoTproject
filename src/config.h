#ifndef CONFIG_H
#define CONFIG_H

// driver layer
// 1. motor and sensor
#define MOTOR_PINS {2,3,6,13}
#define OPTO_SENSOR_PIN 28
#define PIEZO_SENSOR_PIN 27

// 2. EEPROM and I2C0
#define I2C_PORT i2c0
#define EEPROM_SDA_GPIO 16
#define EEPROM_SCL_GPIO 17

// 3.OLED and I2C1
#define OLED_I2C_PORT i2c1
#define OLED_SDA_GPIO 14
#define OLED_SCL_GPIO 15
#define OLED_ADDR 0x3C

// 4. buttons and leds
#define SW0_GPIO 9 //control LED0
#define SW1_GPIO 8 //control LED1
#define SW2_GPIO 7 //control LED2

#define LED0_GPIO 22
#define LED1_GPIO 21
#define LED2_GPIO 20

#define LED_BIT_0 (1 << 0)  // 0b00000001
#define LED_BIT_1 (1 << 1)  // 0b00000010 = 2
#define LED_BIT_2 (1 << 2) // 0b0000100 = 4

#define ENCODER_A_GPIO 10
#define ENCODER_B_GPIO 11
#define ENCODER_SW_GPIO 12

// 5. LoRa
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define BAUD_RATE 9600


// logic layer
// at least 80ms for a pill to fall through
#define PILL_FALL_TIMEOUT_MS 150
// user could define how long the period between 1-7 days
#define MAX_PERIOD 7
#define DEFAULT_PERIOD 7
#define WELCOME_PAGE_TIMEOUT 2000
#define PILL_DISPENSE_INTERVAL 10000 // if success, change to 30s when demo it.
#define PILL_DISPENSE_RETRY 3000 //if any compartments without a pill, try to turn to another compartment every 3s
#define MAX_DISPENSE_RETRIES 7 // if tried 7 times after still no pill comes out, that means the dispensr is empty
#define POWER_ON_WARNING_TIME 10000

#endif
