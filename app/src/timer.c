#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "rs485_handlers.h"
#include "message.h"
#include "worker.h"
#include "timer.h"


LOG_MODULE_REGISTER(timer, LOG_LEVEL_INF);


void timer_heartbeat_expiry_cb(struct k_timer *timer_id)
{
    worker_t *worker = CONTAINER_OF(timer_id, worker_t, timer_heartbeat);

    struct tx_msg tx_msg;
    memset(&tx_msg, 0, sizeof(tx_msg));
    tx_msg.data[0] = 0xAA;
    tx_msg.data[1] = 0x55;
    tx_msg.data[2] = 0x00;
    tx_msg.data[3] = worker->local_id;  
    tx_msg.data[4] = 0x00;
    tx_msg.data[5] = 0x00;
    tx_msg.data[6] = 0x00;
    tx_msg.data[7] = Xor(tx_msg.data, 8);
    tx_msg.len = 8;
    if (k_msgq_put(worker->tx_msg, &tx_msg, K_NO_WAIT) != 0) {
        LOG_WRN("TX queue full");
    } else {
        LOG_INF("reply enqueued");
    }
}

void timer_action_expiry_cb(struct k_timer *timer_id)
{
    worker_t *worker = CONTAINER_OF(timer_id, worker_t, timer_action);

    worker->action_id = 0;
    const char *msg = "Timer expired!";
    LOG_INF("Timer expired, sending message");
    rs485_send((const uint8_t *)msg, strlen(msg));
}
