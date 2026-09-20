#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>
#include <stddef.h>
#include <zephyr/kernel.h>

#define CMD_SOURCE_BT     0
#define CMD_SOURCE_ZIGBEE 1
#define CMD_SOURCE_RS485  2

typedef struct {
    struct k_timer timer_heartbeat;
    struct k_timer timer_action;
    struct k_msgq *tx_msg;
    uint8_t local_id;
    uint8_t action_id;
    uint8_t controller_id;
    uint8_t source;
    bool is_connected;
    bool is_ready;
} worker_t;

typedef struct{
    uint8_t id;
    uint8_t (*handler)(worker_t *worker,uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len);
} command_handler_t;

worker_t *get_worker(void);
void free_worker(worker_t *worker);

uint8_t do_work(worker_t *worker, uint8_t *data, size_t len,
                 uint8_t *reply, size_t *reply_len);
uint8_t Xor(const void* data, int length);

#endif