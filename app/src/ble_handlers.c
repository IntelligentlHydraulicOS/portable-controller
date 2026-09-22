#include "ble_handlers.h"
#include "worker.h"
#include "rs485_handlers.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "message.h"

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* ---------- NUS 实例定义（Zephyr 原生 API） ---------- */
BT_NUS_INST_DEFINE(nus_inst);

/* ---------- 连接与订阅状态 ---------- */
static struct bt_conn *current_conn = NULL;
static bool nus_tx_notif_enabled = false;
static bool notify_subscribed = false;
static bool indicate_subscribed = false;

/* ---------- 广播数据 ---------- */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

/* ---------- 消息队列 ---------- */
K_MSGQ_DEFINE(bt_rx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(bt_tx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);

struct k_msgq *get_ble_tx_msgq(void)
{
    return &bt_tx_msgq;
}

/* ---------- 自定义 GATT 服务：Notify + Indicate ---------- */
#define BT_UUID_CUSTOM_SVC_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BT_UUID_CUSTOM_NOTIFY_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define BT_UUID_CUSTOM_INDICATE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

static struct bt_uuid_128 custom_svc_uuid =
    BT_UUID_INIT_128(BT_UUID_CUSTOM_SVC_VAL);
static struct bt_uuid_128 custom_notify_uuid =
    BT_UUID_INIT_128(BT_UUID_CUSTOM_NOTIFY_VAL);
static struct bt_uuid_128 custom_indicate_uuid =
    BT_UUID_INIT_128(BT_UUID_CUSTOM_INDICATE_VAL);

static void custom_notify_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                          uint16_t value)
{
    ARG_UNUSED(attr);
    notify_subscribed = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Custom Notify %s", notify_subscribed ? "subscribed" : "unsubscribed");
}

static void custom_indicate_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                            uint16_t value)
{
    ARG_UNUSED(attr);
    indicate_subscribed = (value == BT_GATT_CCC_INDICATE);
    LOG_INF("Custom Indicate %s", indicate_subscribed ? "subscribed" : "unsubscribed");
}

BT_GATT_SERVICE_DEFINE(custom_svc,
    BT_GATT_PRIMARY_SERVICE(&custom_svc_uuid),
    BT_GATT_CHARACTERISTIC(&custom_notify_uuid.uuid,
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE,
                           NULL, NULL, NULL),
    BT_GATT_CCC(custom_notify_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&custom_indicate_uuid.uuid,
                           BT_GATT_CHRC_INDICATE,
                           BT_GATT_PERM_NONE,
                           NULL, NULL, NULL),
    BT_GATT_CCC(custom_indicate_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static inline const struct bt_gatt_attr *get_notify_attr(void)
{
    return &custom_svc.attrs[2];
}

static inline const struct bt_gatt_attr *get_indicate_attr(void)
{
    return &custom_svc.attrs[5];
}

/* ---------- Indicate 完成回调 ---------- */
static void indicate_cb(struct bt_conn *conn,
                        struct bt_gatt_indicate_params *params,
                        uint8_t err)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(params);
    if (err) {
        LOG_ERR("Indicate failed (err %u)", err);
    } else {
        LOG_INF("Indicate ACK received");
    }
}

/* ---------- 统一发送接口 ---------- */
int ble_send(ble_tx_type_t type, const uint8_t *data, uint16_t len)
{
    struct bt_conn *conn = current_conn;
    int err = 0;

    if (!conn) {
        LOG_WRN("BLE not connected");
        return -ENOTCONN;
    }

    switch (type) {
    case BLE_TX_NUS:
        if (!nus_tx_notif_enabled) {
            LOG_WRN("NUS TX not subscribed");
            return -EACCES;
        }
        /* Zephyr 原生 API: conn 在前, inst 在后 */
        err = bt_nus_inst_send(conn, &nus_inst, data, len);
        break;

    case BLE_TX_NOTIFY:
        if (!notify_subscribed) {
            LOG_WRN("Custom Notify not subscribed");
            return -EACCES;
        }
        err = bt_gatt_notify(conn, get_notify_attr(), data, len);
        break;

    case BLE_TX_INDICATE:
        if (!indicate_subscribed) {
            LOG_WRN("Custom Indicate not subscribed");
            return -EACCES;  
        }
        {
            /* 必须 static：bt_gatt_indicate 异步引用该结构 */
            static struct bt_gatt_indicate_params ind_params;
            ind_params.attr = get_indicate_attr();
            ind_params.data = data;
            ind_params.len  = len;
            ind_params.func = indicate_cb;
            err = bt_gatt_indicate(conn, &ind_params);
        }
        break;

    default:
        return -EINVAL;
    }

    if (err < 0) {
        LOG_DBG("ble_send(type=%d) fail: %d", type, err);
    } else {
        LOG_DBG("ble_send(type=%d) OK, %u bytes", type, len);
    }
    return err;
}

/* ---------- NUS 回调 ---------- */
static void nus_notif_enabled(bool enabled, void *ctx)
{
    ARG_UNUSED(ctx);
    nus_tx_notif_enabled = enabled;
    LOG_INF("NUS TX notif %s", enabled ? "Enabled" : "Disabled");
}

static void nus_received(struct bt_conn *conn, const void *data,
                         uint16_t len, void *ctx)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);

    if (!data || len == 0 || len > TX_MSG_MAX_DATA) {
        return;
    }

    struct tx_msg msg;
    memset(&msg, 0, sizeof(msg));
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

/* ---------- 连接管理 ---------- */
static void recycled_cb(void)
{
    int err;
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising restart failed (err %d)", err);
    } else {
        LOG_INF("Advertising restarted successfully");
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("BLE connection failed (err %u)", err);
        return;
    }

    if (current_conn) {
        bt_conn_unref(current_conn);
    }

    current_conn = bt_conn_ref(conn);

    /* 重连后所有订阅状态自动失效 */
    nus_tx_notif_enabled = false;
    notify_subscribed = false;
    indicate_subscribed = false;

    LOG_INF("BLE connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (current_conn == conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    nus_tx_notif_enabled = false;
    notify_subscribed = false;
    indicate_subscribed = false;

    LOG_INF("BLE disconnected (reason %u)", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = recycled_cb,
};

/* ---------- 初始化 ---------- */
int ble_init(void)
{
    int err;

    /* Zephyr 原生 NUS API: (inst, cb, ctx) */
    err = bt_nus_inst_cb_register(&nus_inst, &nus_listener, NULL);
    if (err) {
        LOG_ERR("NUS cb reg fail: %d", err);
        return err;
    }
    LOG_DBG("BLE init bt_nus_inst_cb_register done");

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("BT enable fail: %d", err);
        return err;
    }
    LOG_DBG("BLE init bt_enable done");

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Adv start fail: %d", err);
        return err;
    }
    LOG_INF("BLE init done");
    return 0;
}

uint8_t hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

void to_hex(uint8_t *data, size_t len, uint8_t *out) {
    if (data == NULL || out == NULL) {
        return;
    }
    for (int i = 0; i < len; i += 2) {
        uint8_t high = hex_char_to_val(data[i]);
        uint8_t low  = hex_char_to_val(data[i + 1]);
        out[i / 2] = (high << 4) | low;
    }
}

/* ---------- RX 线程 ---------- */
void ble_rx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg rx_msg;
    memset(&rx_msg, 0, sizeof(rx_msg));
    uint8_t reply[TX_MSG_MAX_DATA];
    size_t reply_len = 0;
    worker_t *worker = get_worker();
    worker->source = CMD_SOURCE_BT;
    worker->tx_msg = get_ble_tx_msgq();

    while (1) {
        if (k_msgq_get(&bt_rx_msgq, &rx_msg, K_FOREVER) != 0) {
            continue;
        }

        LOG_INF("BT RX %u bytes", rx_msg.len);
        LOG_HEXDUMP_INF(rx_msg.data, rx_msg.len, "BT Cmd:");

        rs485_send(rx_msg.data, rx_msg.len);

        if(rx_msg.len == 18){
            to_hex(rx_msg.data, 18, rx_msg.data);
            rx_msg.len = 9;
        }
        int ret = do_work(worker, rx_msg.data, rx_msg.len, reply, &reply_len);

        if (ret == 1 && reply_len > 0) {
            struct tx_msg tx_msg;
            memset(&tx_msg, 0, sizeof(tx_msg));
            memcpy(tx_msg.data, reply, reply_len);
            tx_msg.len = reply_len;
            tx_msg.type = BLE_TX_NOTIFY;   /* 按业务需要切换 */
            if (k_msgq_put(worker->tx_msg, &tx_msg, K_NO_WAIT) != 0) {
                LOG_WRN("TX queue full");
            } else {
                LOG_INF("reply enqueued");
            }
        }
    }
    free_worker(worker);
}

/* ---------- TX 线程 ---------- */
void ble_tx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg msg;
    memset(&msg, 0, sizeof(msg));

    while (1) {
        if (k_msgq_get(&bt_tx_msgq, &msg, K_FOREVER) != 0) {
            continue;
        }

        rs485_send(msg.data, msg.len);

        int err = ble_send(msg.type, msg.data, msg.len);
        if (err < 0) {
            LOG_WRN("ble_send failed, drop %u bytes", msg.len);
        }
    }
}