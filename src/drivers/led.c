#include "led.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define BUTTON_DEBOUNCE_MS 200

static const uint leds[LEDS_COUNT] = {LED0_GPIO, LED1_GPIO, LED2_GPIO};
static int current_brightness = BRIGHTNESS_NORMAL;
static bool is_blinking = false;
static LedMode current_led_mode = LED_ALL_OFF;
static uint32_t last_toggle_time = 0;
static uint32_t last_sw0_time = 0;
static uint32_t last_sw2_time = 0;

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


void buttons_init() {
    gpio_init(SW0_GPIO);gpio_set_dir(SW0_GPIO, GPIO_IN);gpio_pull_up(SW0_GPIO);
    gpio_init(SW1_GPIO);gpio_set_dir(SW1_GPIO, GPIO_IN);gpio_pull_up(SW1_GPIO);
    gpio_init(SW2_GPIO);gpio_set_dir(SW2_GPIO, GPIO_IN);gpio_pull_up(SW2_GPIO);
}

bool is_sw0_pressed(void) {
    bool is_down = !gpio_get(SW0_GPIO);
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (is_down && (now - last_sw0_time > BUTTON_DEBOUNCE_MS)) {
        last_sw0_time = now;
        return true;
    }
    return false;
}

bool is_sw2_pressed(void) {
    bool is_down = !gpio_get(SW2_GPIO);
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (is_down && (now - last_sw2_time > BUTTON_DEBOUNCE_MS)) {
        last_sw2_time = now;
        return true;
    }
    return false;
}
