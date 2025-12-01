#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "stdio.h"
#include "config.h"
#include <string.h>
#include <stdbool.h>

// I2C read/write
void eeprom_write_bytes(uint16_t addr, uint8_t *data_p, size_t length);
void eeprom_read_bytes(uint16_t addr, uint8_t *data_p, size_t length);
uint16_t crc16(const uint8_t *data_p, size_t length);
bool log_entry_is_valid(const uint8_t *buffer);
void log_erase_all();
void log_read_all();
void log_write_message(const char *message);
void user_command_serial_interface();

// ex1 Led state functions
void set_led_state(ledstate *ls, uint8_t value);
bool led_state_is_valid(ledstate *ls);
void update_leds(uint8_t state);
void gpio_callback(uint gpio, uint32_t events);
void eeprom_write_ledstate(uint16_t addr, ledstate *ls);
void eeprom_read_ledstate(uint16_t addr, ledstate *ls);

// why volatile?
// because these variables are modified inside an interrupt handler (gpio_callback) and main().
static volatile uint8_t current_leds = 0;
static volatile bool state_changed = false;
static volatile uint64_t last_interrupt_time = 0;


int main() {
    stdio_init_all();
    sleep_ms(1000);
    printf("Ex2: Log Management.\n");
    printf("Commands accepted: 'read','erase'.\n");
    printf("----------------------------------\n");
    i2c_init(I2C_PORT, 100000);
    gpio_set_function(SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_GPIO);
    gpio_pull_up(SCL_GPIO);

    gpio_init(SW0_GPIO);gpio_set_dir(SW0_GPIO, GPIO_IN);gpio_pull_up(SW0_GPIO);
    gpio_init(SW1_GPIO);gpio_set_dir(SW1_GPIO, GPIO_IN);gpio_pull_up(SW1_GPIO);
    gpio_init(SW2_GPIO);gpio_set_dir(SW2_GPIO, GPIO_IN);gpio_pull_up(SW2_GPIO);

    gpio_init(LED0);gpio_set_dir(LED0, GPIO_OUT);
    gpio_init(LED1);gpio_set_dir(LED1, GPIO_OUT);
    gpio_init(LED2);gpio_set_dir(LED2, GPIO_OUT);

    gpio_set_irq_enabled_with_callback(SW0_GPIO, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(SW1_GPIO, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(SW2_GPIO, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    ledstate my_ledstate;
    //printf("Reading from EEPROM, state stored at address:%d.\n", STORE_LEDSTATE_ADDR);

    eeprom_read_ledstate(STORE_LEDSTATE_ADDR, &my_ledstate);


    // first check if there are some trash in eeprom
    if (led_state_is_valid(&my_ledstate)) {
        current_leds = my_ledstate.state;
        //printf("Validation OK! State found...\n");
        printf(">> Last valid state: 0x%02X\n", current_leds);
    } else {
        current_leds = DEFAULT_STATE;
        //printf("Validation ERROR! State not found...\n");
        printf(">> Using default: 0x%02X\n", current_leds);
    }
    //printf("------------------------------\n");

    // set leds to the current state(from eeprom last time or default)
    update_leds(current_leds);

    log_write_message("Boot.");

    uint64_t power_up_seconds = time_us_64()/1000000;
    //printf("[%llu s] from system boot. Current State:0x%02X.\n", power_up_seconds, current_leds);
   // printf("----------------------------------\n");

    while (1) {
        user_command_serial_interface();

        if (state_changed) {
            state_changed = false;
            update_leds(current_leds);

            uint64_t current_time = time_us_64()/1000000;
            printf("[%llu s] from first validation. New State:0x%02X.\n", current_time, current_leds);
            set_led_state(&my_ledstate, current_leds);
            eeprom_write_ledstate(STORE_LEDSTATE_ADDR, &my_ledstate);
            printf("----------------------------------\n");

            char log_msg[INPUT_BUFFER_SIZE];
            snprintf(log_msg, INPUT_BUFFER_SIZE, "LED State changed to 0x%02X", current_leds);
            log_write_message(log_msg);
            printf("==================================\n");
        }
    }
}

// Exercise 2 Log storage functions
uint16_t crc16(const uint8_t *data_p, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF; // Initial value
    while (length--) {
        x = crc >>8 ^ *data_p++;
        x ^= x >>4;
        crc = (crc <<8) ^ ((uint16_t)(x <<12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}

//A log entry consists of a string that contains maximum 61 characters,
//a terminating null character(zero) and two-byte CRC that is used to validate the integrity of the data.
//The string must contain at least one character.
bool log_entry_is_valid(const uint8_t *buffer) {
    //The string must contain at least one character.
    if (buffer[0]==0) return false;

    //a terminating null character "\0"
    //define an unused number in the buffer entry.
    uint8_t null_position=-1;
    int i=0;
    while (i<62 &&buffer[i]!='\0') {
        i++;
    }
    if (i==62) return false; //no null found in first 62 bytes
    null_position=i;

    int crc_position = null_position+1;
    if (crc_position+1>=LOG_ENTRY_SIZE) return false; //no space for crc

    // crc validation
    // string+'\0'+crc
    size_t total_length = null_position+3;
    uint16_t crc16_result = crc16(buffer, total_length);
    if (crc16_result!=0) {
        return false;
    }
    return true;
}

void log_erase_all() {
    printf("Erasing all logs...\n");
    uint8_t zero_at_first_byte = 0;
    for (uint16_t i=0; i<LOG_MAX_ENTRIES; i++) {
        uint16_t address = LOG_BASE_ADDRESS +(i* LOG_ENTRY_SIZE);
        eeprom_write_bytes(address, &zero_at_first_byte, 1);
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

    for (int i=0;i<LOG_MAX_ENTRIES;i++) {
        uint16_t address = LOG_BASE_ADDRESS + (i* LOG_ENTRY_SIZE);
        eeprom_read_bytes(address, &buffer[0], LOG_ENTRY_SIZE);
        if (!log_entry_is_valid(buffer)) {
            target_entry_index = i;
            break;
        }
    }
    // entry_index remains -1, means i go through all position for entries,and every one is valid.
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

void user_command_serial_interface() {
    static char input_buffer[INPUT_BUFFER_SIZE];
    static int index=0;
    //non-blocking check for user input
    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) return;

    if (c=='\n'||c=='\r') {
        input_buffer[index]='\0';
        printf("\n");
        if (strcmp(input_buffer,"read")==0) {
            log_read_all();
        }else if (strcmp(input_buffer,"erase")==0) {
            log_erase_all();
        }else if (strlen(input_buffer)>0) {
            printf("Invalid command.\n");
        }
        index=0;
    }
    else if (c >= 32 && c <= 126 && index < INPUT_BUFFER_SIZE - 1){
        input_buffer[index] = (char)c;
        index++;
    }
    // QUESTION HERE: how to handle backspace correctly in terminal?
    else if ((c == 8 || c == 127) && index > 0) {
        index--;
        input_buffer[index] = '\0';
        printf("\b \b");
    }
}

// I2C read/write helper functions
void eeprom_write_bytes(uint16_t addr, uint8_t *data_p, size_t length) {
    uint8_t buf[2 + length];
    buf[0] = (uint8_t)(addr >> 8);
    buf[1] = (uint8_t)(addr & 0xFF);
    for (size_t i = 0; i < length; i++) {
        buf[2 + i] = data_p[i];
    }
    i2c_write_blocking(I2C_PORT, EEPROM_ADDR, buf, length+2, false);
    sleep_ms(5);
}

void eeprom_read_bytes(uint16_t addr, uint8_t *data_p, size_t length) {
    uint8_t addr_buf[2];
    addr_buf[0] = (uint8_t)(addr >> 8);
    addr_buf[1] = (uint8_t)(addr & 0xFF);
    i2c_write_blocking(I2C_PORT, EEPROM_ADDR, addr_buf, 2, true);
    i2c_read_blocking(I2C_PORT, EEPROM_ADDR, data_p, length, false);
}

// exercise 1 helper functions
void set_led_state(ledstate *ls, uint8_t value)
{
    ls->state = value;
    ls->not_state = ~value;
}
// check another round before use. if the ~not_state matches state, then it's valid.
bool led_state_is_valid(ledstate *ls) {
    return ls->state == (uint8_t) ~ls->not_state;
}

void eeprom_write_ledstate(uint16_t addr, ledstate *ls) {
    eeprom_write_bytes(addr, (uint8_t*)ls, sizeof(ledstate));
}

void eeprom_read_ledstate(uint16_t addr, ledstate *ls) {
    eeprom_read_bytes(addr, (uint8_t*)ls, sizeof(ledstate));
}

void update_leds(uint8_t state) {
    // when check the state of each Led, shift to the very right.
    // esim, led1 at first on, state = 0b00000010
    // shift right 1 time, state = 0b00000001
    // then AND with 1, result is 1, so input led1 is on.
    // otherwise, if led1 is off, state = 0b00000000
    // shift right 1 time, state = 0b00000000
    // then AND with 1, result is 0, so input led1 is off
    gpio_put(LED0, (state >> 0) &1);
    gpio_put(LED1, (state >> 1) &1);
    gpio_put(LED2, (state >> 2) &1);
}

void gpio_callback(uint gpio, uint32_t events) {
    //static uint64_t last_interrupt_time = 0;
    uint64_t current_time = time_us_64();

    // debounce: ignore interrupts within 500ms
    if (current_time - last_interrupt_time < 500000) return;
    last_interrupt_time = current_time;

    uint toggle_bit = 0;
    // make sure which button is pressed
    if (gpio==SW0_GPIO) {
        toggle_bit = LED_BIT_0;
    } else if (gpio==SW1_GPIO) {
        toggle_bit = LED_BIT_1;
    } else if (gpio==SW2_GPIO) {
        toggle_bit = LED_BIT_2;
    }
    if (toggle_bit) {
        current_leds ^= toggle_bit;
        state_changed = true;
    }
}
