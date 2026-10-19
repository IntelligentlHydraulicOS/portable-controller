#include <zephyr/logging/log.h>
#include <string.h>
#include <zephyr/kernel.h>
#include "rs485_handlers.h"
#include "message.h"
#include "worker.h"
#include "timer.h"
#include "action.h"
#include "led_strip.h"
#include "bee.h"
#include "led_control.h"

LOG_MODULE_REGISTER(timer, LOG_LEVEL_INF);

static int test_index = 0;
static struct k_timer test_timer;

int init_work_timer(work_timer_t *work_timer, int period_msecond, void (*callback)(struct k_timer *timer))
{
    if (work_timer == NULL || callback == NULL || period_msecond < 0) {
        return -EINVAL;
    }

    work_timer->is_active = false;
    work_timer->period_msecond = period_msecond;
    k_timer_init(&work_timer->timer, callback, NULL);

    return 0;
}

int start_work_timer(work_timer_t *work_timer)
{
    if (work_timer == NULL) {
        return -EINVAL;
    }
    atomic_set(&work_timer->pending_cmds, 1);
    work_timer->is_active = true;
    k_timer_start(&work_timer->timer, K_MSEC(1), K_MSEC(work_timer->period_msecond));
    return 0;
}

int stop_work_timer(work_timer_t *work_timer)
{
    if (work_timer == NULL) {
        return -EINVAL;
    }

    if (work_timer->is_active) {
        k_timer_stop(&work_timer->timer);
        work_timer->is_active = false;
    }
    atomic_set(&work_timer->pending_cmds, 0);

    return 0;
}

void timer_heartbeat_expiry_cb(struct k_timer *timer)
{
    work_timer_t *wt = CONTAINER_OF(timer, work_timer_t, timer);
    worker_t *worker = CONTAINER_OF(wt, worker_t, timer_heartbeat);

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

void timer_action_expiry_cb(struct k_timer *timer)
{
    work_timer_t *wt = CONTAINER_OF(timer, work_timer_t, timer);
    worker_t *worker = CONTAINER_OF(wt, worker_t, timer_action);

    bool from_cmd = atomic_cas(&wt->pending_cmds, 1, 0);
    if(from_cmd){
        LOG_DBG("Action timer triggered by command");
        uint8_t channel_offset = get_channel_offset_by_action_id(worker->action_id);
        LOG_INF("channel_offset: %u", channel_offset);
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
    }else{
        LOG_DBG("Action timer triggered by expiry");
        if (wt->is_active) {
            stop_work_timer(&worker->timer_action);
        }
    }
}

void timer_bee_expiry_cb(struct k_timer *timer)
{    
    ARG_UNUSED(timer);
    work_timer_t *wt = CONTAINER_OF(timer, work_timer_t, timer);
    worker_t *worker = CONTAINER_OF(wt, worker_t, timer_bee);

    bool from_cmd = atomic_cas(&wt->pending_cmds, 1, 0);
    if(from_cmd){
        LOG_DBG("Bee timer triggered by command");
        //TODO just for test
        led_control_blink_alt(0, 255, 0,   0, 0, 255,   250);
        buzzer_on();
    }else{
        LOG_DBG("Bee timer triggered by expiry");
        buzzer_off();
        stop_work_timer(&worker->timer_bee);
        // TODO just for test
        led_control_static(0, 255, 0);
    }
}

void test_timer_action_expiry_cb(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    struct tx_msg tx_msg;

    int channel = 1 << test_index++;
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
        if (test_index >= 28) {
            test_index = 0; // Reset test_index after reaching 28
        }
        k_sleep(K_SECONDS(1));
    }
}