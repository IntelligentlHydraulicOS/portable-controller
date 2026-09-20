#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "rs485_handlers.h"
#include "message.h"
#include "worker.h"
#include "timer.h"
#include "action.h"

LOG_MODULE_REGISTER(timer, LOG_LEVEL_INF);

static int index = 0;
static struct k_timer test_timer;

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
    tx_msg.type = BLE_TX_NUS;
    if (k_msgq_put(worker->tx_msg, &tx_msg, K_NO_WAIT) != 0) {
        LOG_WRN("TX queue full");
    } else {
        LOG_DBG("reply enqueued");
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
        struct tx_msg tx_msg;
        memset(&tx_msg.data, 0, TX_MSG_MAX_DATA);
        tx_msg.data[0] = 0x55;
        tx_msg.data[1] = 0xAA;
        tx_msg.data[2] = 0x00;
        tx_msg.data[3] = 0x09;  
        tx_msg.data[4] = 0x00;
        tx_msg.data[5] = 0x00;
        tx_msg.data[6] = 0x00;
        tx_msg.data[7] = 0x00;
        setControlTxBuf(tx_msg.data + 4, channel);
        tx_msg.data[8] = Checksum(tx_msg.data, 8);
        tx_msg.len = 9;
        if (k_msgq_put(get_rs485_tx_msgq(), &tx_msg, K_NO_WAIT) != 0) {
            LOG_WRN("TX queue full");
        } else {
            LOG_DBG("reply enqueued");
        }
    } else {
        LOG_WRN("Invalid action_id: %u", worker->action_id);
    }
}

void test_timer_action_expiry_cb(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    struct tx_msg tx_msg;

    int channel = 1 << index++;
    memset(&tx_msg.data, 0, TX_MSG_MAX_DATA);
    tx_msg.data[0] = 0x55;
    tx_msg.data[1] = 0xAA;
    tx_msg.data[2] = 0x00;
    tx_msg.data[3] = 0x09;  
    tx_msg.data[4] = 0x00;
    tx_msg.data[5] = 0x00;
    tx_msg.data[6] = 0x00;
    tx_msg.data[7] = 0x00;
    setControlTxBuf(tx_msg.data + 4, channel);
    tx_msg.data[8] = Checksum(tx_msg.data, 8);
    tx_msg.len = 9;
    if (k_msgq_put(get_rs485_tx_msgq(), &tx_msg, K_NO_WAIT) != 0) {
        LOG_WRN("TX queue full");
    } else {
        LOG_DBG("reply enqueued");
    }
}

void timer_test_thread(void *args)
{
    k_timer_init(&test_timer, test_timer_action_expiry_cb, NULL);
    k_timer_start(&test_timer, K_SECONDS(1), K_SECONDS(30));

    while (1) {
        if (index >= 28) {
            index = 0; // Reset index after reaching 28
        }
        k_sleep(K_SECONDS(1));
    }
}