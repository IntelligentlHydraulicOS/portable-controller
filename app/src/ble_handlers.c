#include "ble_handlers.h"
#include "worker.h"
#include "rs485_handlers.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "message.h"

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

K_MSGQ_DEFINE(bt_rx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(bt_tx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);

extern struct k_msgq *zigbee_tx_msgq_get(void);

static void nus_notif_enabled(bool enabled, void *ctx) {
    ARG_UNUSED(ctx);
    LOG_INF("NUS %s", enabled ? "Enabled" : "Disabled");
}

static void nus_received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx) {
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);

    if (!data || len == 0 || len > TX_MSG_MAX_DATA) {
        return;
    }

    struct tx_msg msg;
    memcpy(msg.data, data, len);
    msg.len = len;

    if (k_msgq_put(&bt_rx_msgq, &msg, K_NO_WAIT) != 0) {
        LOG_WRN("BT RX queue full");
    }
}

static struct bt_nus_cb nus_listener = {
    .notif_enabled = nus_notif_enabled,
    .received = nus_received,
};

void ble_init(void) {
    int err;

    err = bt_nus_cb_register(&nus_listener, NULL);
    if (err) {
        LOG_ERR("NUS cb reg fail: %d", err);
        return;
    }

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("BT enable fail: %d", err);
        return;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Adv start fail: %d", err);
    }
}

void ble_rx_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg rx_msg;
    uint8_t reply[TX_MSG_MAX_DATA];
    size_t reply_len;
    worker_t *worker = get_worker();
    worker->source = CMD_SOURCE_BT;
    worker->tx_msg = get_bt_tx_msgq();

    while (1) {
        if (k_msgq_get(&bt_rx_msgq, &rx_msg, K_FOREVER) != 0) {
            continue;
        }

        LOG_INF("BT RX %u bytes", rx_msg.len);
        LOG_HEXDUMP_INF(rx_msg.data, rx_msg.len, "BT Cmd:");
 
        int ret = do_work(worker, rx_msg.data, rx_msg.len, reply, &reply_len);

        if (ret == 1) {
            if (rx_msg.len > 1) {
                struct tx_msg zigbee_msg;
                zigbee_msg.len = rx_msg.len - 1;
                memcpy(zigbee_msg.data, rx_msg.data + 1, zigbee_msg.len);
                struct k_msgq *zigbee_tx = zigbee_tx_msgq_get();
                if (k_msgq_put(zigbee_tx, &zigbee_msg, K_NO_WAIT) == 0) {
                    snprintf((char *)reply, TX_MSG_MAX_DATA, "ZIG:SENT");
                    reply_len = strlen((char *)reply);
                } else {
                    snprintf((char *)reply, TX_MSG_MAX_DATA, "ZIG:QUEUE_FULL");
                    reply_len = strlen((char *)reply);
                }
            } else {
                snprintf((char *)reply, TX_MSG_MAX_DATA, "ZIG:NO_DATA");
                reply_len = strlen((char *)reply);
            }
        } else if (ret < 0) {
            snprintf((char *)reply, TX_MSG_MAX_DATA, "PARSE_ERROR");
            reply_len = strlen((char *)reply);
        }

        if (reply_len > 0) {
            struct tx_msg tx_msg;
            memcpy(tx_msg.data, reply, reply_len);
            tx_msg.len = reply_len;
            if (k_msgq_put(worker->tx_msg, &tx_msg, K_NO_WAIT) != 0) {
                LOG_WRN("TX queue full");
            } else {
                LOG_INF("reply enqueued");
            }
        }
    }
    free_worker(worker);
}

void ble_tx_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg msg;

    while (1) {
        if (k_msgq_get(&bt_tx_msgq, &msg, K_FOREVER) != 0) {
            continue;
        }

        int err = bt_nus_send(NULL, msg.data, msg.len);
        if (err < 0) {
            LOG_ERR("bt_nus_send fail: %d", err);
        } else {
            LOG_INF("BT sent %u bytes", msg.len);
            LOG_HEXDUMP_INF(msg.data, msg.len, "BT Sent:");
        }
    }
}

struct k_msgq *get_bt_tx_msgq(void) {
    return &bt_tx_msgq;
}