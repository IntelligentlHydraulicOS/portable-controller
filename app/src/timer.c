#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "rs485_handlers.h"
#include "message.h"
#include "worker.h"
#include "timer.h"
#include "action.h"

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

uint8_t Checksum(uint8_t *data, int iSize)
{
	uint16_t uiSum = 0;
	for(int i = 0; i < iSize; i++)
	{
		uiSum += data[i];
	}

	return (uiSum & 0xff);
}

void setControlTxBuf(uint8_t *uiBuf, uint32_t channel)
{
	uiBuf[0] = (channel >> 24) & 0xff;
	uiBuf[1] = (channel >> 16) & 0xff;
	uiBuf[2] = (channel >> 8) & 0xff;
	uiBuf[3] = channel & 0xff;
}

void timer_action_expiry_cb(struct k_timer *timer_id)
{
    worker_t *worker = CONTAINER_OF(timer_id, worker_t, timer_action);

    uint8_t channel_offset = get_channel_offset_by_action_id(worker->action_id);
    if (channel_offset != 0xFF) {
        uint32_t channel = 1 << channel_offset;
        uint8_t data[9] = {0x55, 0xAA, 0x0, 0x09, 0x00, 0x00, 0x00, 0x00};
        setControlTxBuf(data + 4, channel);
        data[8] = Checksum(data, 8);
        rs485_send((const uint8_t *)data, 9 * sizeof(uint8_t));
        LOG_INF("Action %u sent on channel %u", worker->action_id, channel);
    } else {
        LOG_WRN("Invalid action ID %u", worker->action_id);
    }
}
