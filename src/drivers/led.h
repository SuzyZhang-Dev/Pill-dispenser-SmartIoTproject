//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_LED_H
#define PILLDISPENSER_LED_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    LED_ALL_OFF,
    LED_ALL_ON,
    LED_BLINKING
}LedMode;

#define LEDS_COUNT 3
#define BRIGHTNESS_MAX 1000 //WRAP
#define LED_DIVIDER 125
#define BRIGHTNESS_NORMAL 200 // don't be so bright
#define BRIGHTNESS_ERROR_OCCUR 800
#define BLINK_INTERVAL_MS 500
#define BlINK_INTERVAL_RECALIB 100

void led_init(void);
void leds_set_brightness(uint16_t brightness);
void led_set_mode(LedMode mode);
void led_blink_task(void);
void led_blinking_error(int times, int interval);


#endif //PILLDISPENSER_LED_H