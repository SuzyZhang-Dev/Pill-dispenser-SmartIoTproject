#include "led.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

static const uint leds[LEDS_COUNT] = {LED0_GPIO, LED1_GPIO, LED2_GPIO};
static int current_brightness = BRIGHTNESS_NORMAL;
static bool is_blinking = false;
static LedMode current_led_mode = LED_ALL_OFF;

static uint32_t last_toggle_time = 0;


// for adjusting leds brightness
static void set_pwm_level(uint16_t level) {
    for (int i = 0; i < LEDS_COUNT; i++) {
        uint slice = pwm_gpio_to_slice_num(leds[i]);
        uint channel = pwm_gpio_to_channel(leds[i]);
        pwm_set_chan_level(slice, channel, level);
    }
}

void led_init(void) {
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&config, LED_DIVIDER);
    pwm_config_set_wrap(&config, BRIGHTNESS_MAX - 1);

    for (int i = 0; i < LEDS_COUNT; i++) {
        uint slice = pwm_gpio_to_slice_num(leds[i]);
        uint channel = pwm_gpio_to_channel(leds[i]);
        gpio_set_function(leds[i], GPIO_FUNC_PWM);
        pwm_set_enabled(slice, false);
        pwm_init(slice, &config, false);
        pwm_set_chan_level(slice, channel, 0);
        pwm_set_enabled(slice, true);
    }

    current_led_mode = LED_ALL_OFF;
}

void leds_set_brightness(uint16_t brightness) {
    if (brightness > BRIGHTNESS_MAX) brightness = BRIGHTNESS_MAX;
    if (brightness < 0) brightness = 0;

    current_brightness = brightness;
    if (current_led_mode == LED_ALL_ON) {
        set_pwm_level(current_brightness);
    }
}

void led_set_mode(LedMode mode) {
    current_led_mode = mode;
    switch (mode) {
        case LED_ALL_OFF:
            set_pwm_level(0);
            is_blinking = false;
            break;
        case LED_ALL_ON:
            set_pwm_level(current_brightness);
            is_blinking = false;
            break;
        case LED_BLINKING:
            is_blinking = true;
            break;
    }
}

void led_blink_task(void) {
    if (current_led_mode != LED_BLINKING) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_toggle_time >= BLINK_INTERVAL_MS) {
        last_toggle_time = now;
        is_blinking = !is_blinking;
        set_pwm_level(is_blinking ? current_brightness : 0);
    }
}

void led_blinking_nonblocking(int times, int interval) {
    uint32_t old_brightness = current_brightness; // save normal brightness to a new
    LedMode old_mode = current_led_mode;
    led_set_mode(LED_ALL_OFF);
    leds_set_brightness(BRIGHTNESS_ERROR_OCCUR);
    for (int i = 0; i < times; i++) {
        set_pwm_level(BRIGHTNESS_ERROR_OCCUR);
        sleep_ms(interval);
        set_pwm_level(0);
        sleep_ms(interval);
    }
    led_set_mode(old_mode);
    leds_set_brightness(old_brightness);
}
