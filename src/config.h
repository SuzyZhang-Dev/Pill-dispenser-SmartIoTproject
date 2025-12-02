#ifndef CONFIG_H
#define CONFIG_H

// driver layer
// 1. motor and sensor
#define MOTOR_PINS {2,3,6,13}
#define OPTO_SENSOR_PIN 28
#define PIEZO_SENSOR_PIN 27

// 2. EEPROM and I2C
#define I2C_PORT i2c0
#define SDA_GPIO 16
#define SCL_GPIO 17



// 3. buttons and leds
#define SW0_GPIO 9 //control LED0
#define SW1_GPIO 8 //control LED1
#define SW2_GPIO 7 //control LED2

#define LED0 22
#define LED1 21
#define LED2 20

#define LED_BIT_0 (1 << 0)  // 0b00000001
#define LED_BIT_1 (1 << 1)  // 0b00000010 = 2
#define LED_BIT_2 (1 << 2) // 0b0000100 = 4


//=================================
// logic layer

#define PILL_FALL_TIMEOUT_MS 150 // at least 80ms for a pill to fall through



#endif
