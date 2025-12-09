#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "iuart.h"
#include "config.h"
#include "lora.h"
#include "appkey.h"

#define RX_BUFFER_SIZE 128
#define RESPONSE_TIMEOUT_MS 5000 //waits for response for 5s
#define MAX_AT_RETRIES 5 //try 5 times and if not, back to STEP 1


// separate step statemachine and lora-status machine for a clearer logic
// LoraStepJoining_t only take charge of preparing to join.
// LoraStatus holds for joined or not.
typedef enum {
    LORA_STEP_SEND_AT,
    LORA_STEP_WAIT_AT_RESPONSE,
    LORA_STEP_SEND_MODE,
    LORA_STEP_WAIT_MODE_RESPONSE,
    LORA_STEP_SEND_KEY,
    LORA_STEP_WAIT_KEY_RESPONSE,
    LORA_STEP_SEND_CLASS,
    LORA_STEP_WAIT_CLASS_RESPONSE,
    LORA_STEP_SEND_PORT,
    LORA_STEP_WAIT_PORT_RESPONSE,
    LORA_STEP_SEND_JOIN,
    LORA_STEP_WAIT_JOIN_RESPONSE,
    LORA_STEP_READY
} LoraStepJoining_t;


static LoraStepJoining_t lora_step_joining = LORA_STEP_SEND_AT;
static LoraStatus_t lora_status = LORA_STATUS_DISCONNECTED;
static char rx_line_buffer[RX_BUFFER_SIZE];
static int at_retries = 0;
static int rx_pos = 0 ;
static uint32_t step_start_time_ms =0;

// helper functions
// send_command for "AT+" COMMAND
void lora_send_command(const char *cmd) {
    rx_pos = 0;
    iuart_send(UART_NR,cmd);
    iuart_send(UART_NR,"\r\n");
    printf("[LoRa Tx] %s\n",cmd);
    step_start_time_ms = to_ms_since_boot(get_absolute_time());
}

bool lora_read_response() {
    uint8_t ch;
    while (iuart_read(UART_NR,&ch,1)>0) {
        if (rx_pos < RX_BUFFER_SIZE - 1) {
            if (ch != '\r') {
                rx_line_buffer[rx_pos++] = (char)ch;
            }
            if (ch == '\n') {
                rx_line_buffer[rx_pos] = '\0';
                return true;
            }
        }else {
            rx_pos = 0;
        }
    }
    return false;
}

void lora_init() {
    iuart_setup(UART_NR,UART_TX_PIN,UART_RX_PIN,BAUD_RATE);
    sleep_ms(1000);

    lora_step_joining = LORA_STEP_SEND_AT;
    lora_status = LORA_STATUS_CONNECTING;
    printf("[LoRa] Initializing LoRa module...\n");
}

LoraStatus_t lora_get_status() {
    return lora_status;
}

// send_message for application layer sending message like "Pill: 1/7"
// only use after joined LoRa successfully
bool lora_send_message(const char *msg) {
    if (lora_status != LORA_STATUS_JOINED) {
        return false;
    }
    char cmd[RX_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "AT+MSG=\"%s\"", msg);
    lora_send_command(cmd);
    //printf("[LoRa Tx]%s\n",msg);
    return true;
}

void lora_get_ready_to_join() {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (lora_read_response()) {
        printf("[LoRa Rx] %s",rx_line_buffer);

        if (lora_step_joining == LORA_STEP_WAIT_JOIN_RESPONSE) {
            // document P36
            if (strstr(rx_line_buffer, "+JOIN: NetID")) {
                printf("[LoRa Debug] Joined Successful.");
                lora_step_joining = LORA_STEP_READY;
                lora_status = LORA_STATUS_JOINED;
            }
            else if (strstr(rx_line_buffer, "+JOIN: Join failed")) {
                printf("[LoRa Debug] Join failed.");
                step_start_time_ms = now;
                lora_step_joining = LORA_STEP_SEND_JOIN;
            }
        }

        if (strstr(rx_line_buffer, "+AT: OK") ||
            strstr(rx_line_buffer, "+MODE:") ||
            strstr(rx_line_buffer, "+PORT:") ||
            strstr(rx_line_buffer, "+KEY:") ||
            strstr(rx_line_buffer, "+CLASS:")) {

            switch (lora_step_joining) {
                case LORA_STEP_WAIT_AT_RESPONSE:    lora_step_joining = LORA_STEP_SEND_MODE; break;
                case LORA_STEP_WAIT_MODE_RESPONSE:  lora_step_joining = LORA_STEP_SEND_KEY; break;
                case LORA_STEP_WAIT_KEY_RESPONSE:   lora_step_joining = LORA_STEP_SEND_CLASS; break;
                case LORA_STEP_WAIT_CLASS_RESPONSE: lora_step_joining = LORA_STEP_SEND_PORT; break;
                case LORA_STEP_WAIT_PORT_RESPONSE:  lora_step_joining = LORA_STEP_SEND_JOIN; break;
                default: break;
            }
            at_retries = 0;
            }

        rx_pos = 0;
    }

    switch (lora_step_joining) {
        case LORA_STEP_SEND_AT:
            lora_send_command("AT");
            lora_step_joining = LORA_STEP_WAIT_AT_RESPONSE;
            break;

        case LORA_STEP_WAIT_AT_RESPONSE:
            if (now - step_start_time_ms > RESPONSE_TIMEOUT_MS) {
                if (++at_retries < MAX_AT_RETRIES) {
                    printf("Timeout AT, retrying...\n");
                    lora_step_joining = LORA_STEP_SEND_AT;
                } else {
                    printf("LoRa Module not responding!\n");
                    lora_status = LORA_STATUS_FAILED;
                }
            }
            break;

        case LORA_STEP_SEND_MODE:
            lora_send_command("AT+MODE=LWOTAA");
            lora_step_joining = LORA_STEP_WAIT_MODE_RESPONSE;
            break;

        case LORA_STEP_WAIT_MODE_RESPONSE:
            if (now - step_start_time_ms > RESPONSE_TIMEOUT_MS) {
                lora_step_joining = LORA_STEP_SEND_MODE;
            }
            break;

        case LORA_STEP_SEND_KEY:
            {
                char buf[RX_BUFFER_SIZE];
                snprintf(buf, sizeof(buf), "AT+KEY=APPKEY,\"%s\"", APP_KEY);
                lora_send_command(buf);
                lora_step_joining = LORA_STEP_WAIT_KEY_RESPONSE;
            }
            break;

        case LORA_STEP_WAIT_KEY_RESPONSE:
            if (now - step_start_time_ms > RESPONSE_TIMEOUT_MS) {
                lora_step_joining = LORA_STEP_SEND_KEY;
            }
            break;

        case LORA_STEP_SEND_CLASS:
            lora_send_command("AT+CLASS=A");
            lora_step_joining = LORA_STEP_WAIT_CLASS_RESPONSE;
            break;

        case LORA_STEP_WAIT_CLASS_RESPONSE:
            if (now - step_start_time_ms > RESPONSE_TIMEOUT_MS) {
                lora_step_joining = LORA_STEP_SEND_CLASS;
            }
            break;

        case LORA_STEP_SEND_PORT:
            lora_send_command("AT+PORT=8");
            lora_step_joining = LORA_STEP_WAIT_PORT_RESPONSE;
            break;

        case LORA_STEP_WAIT_PORT_RESPONSE:
            if (now - step_start_time_ms > RESPONSE_TIMEOUT_MS) {
                lora_step_joining = LORA_STEP_SEND_PORT;
            }
            break;

        case LORA_STEP_SEND_JOIN:
            lora_send_command("AT+JOIN");
            lora_status = LORA_STATUS_JOINING;
            lora_step_joining = LORA_STEP_WAIT_JOIN_RESPONSE;
            break;

        case LORA_STEP_WAIT_JOIN_RESPONSE:
            if (now - step_start_time_ms > MAX_JOIN_WAITING_TIME_MS) {
                printf("Join Timeout, retrying...\n");
                lora_step_joining = LORA_STEP_SEND_JOIN;
            }
            break;

        case LORA_STEP_READY:
            break;
    }
}