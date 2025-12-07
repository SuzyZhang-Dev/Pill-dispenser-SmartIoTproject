#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "config.h"
#include "dispenser.h"
#include "statemachine.h"
#include "drivers/motor.h"
#include "drivers/oled.h"
#include "drivers/sensor.h"
#include "drivers/led.h"
#include "drivers/encoder&button.h"
#include "drivers/lora.h"

static void system_init() {
    stdio_init_all();
    sleep_ms(2000);
    gpio_set_irq_callback(&statemachine_gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
    led_init();
    buttons_init();
    encoder_init();
    set_motor_pins();
    sensor_init();
    dispenser_init();
    lora_init();
    oled_init();
    oled_init_minimal();
    oled_clear();
    printf("[User] System Init.\n");
}

int main() {
    system_init();
    //watchdog_enable(8000, true);
    sleep_ms(3000);

    while (true) {
        statemachine_loop();
        //watchdog_update();
        sleep_ms(20);
    }

}