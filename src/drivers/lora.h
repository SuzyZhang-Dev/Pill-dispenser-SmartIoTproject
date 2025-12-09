//
// Created by 张悦 on 1.12.2025.
//

#ifndef PILLDISPENSER_LORA_H
#define PILLDISPENSER_LORA_H

typedef enum {
    LORA_STATUS_DISCONNECTED,
    LORA_STATUS_CONNECTING,
    LORA_STATUS_JOINING,
    LORA_STATUS_JOINED,
    LORA_STATUS_FAILED
} LoraStatus_t;

void lora_send_command(const char *cmd);
bool lora_read_response();
void lora_init();
LoraStatus_t lora_get_status();
bool lora_send_message(const char *msg);
void lora_get_ready_to_join();

#define MAX_JOIN_WAITING_TIME_MS 20000 //20 seconds and expose to statemachine.c
#define LORA_RETRY_INTERVAL_MS 10

#endif //PILLDISPENSER_LORA_H