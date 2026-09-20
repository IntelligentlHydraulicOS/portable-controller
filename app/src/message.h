#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdint.h>

#define TX_MSG_QUEUE_SIZE  10
#define TX_MSG_MAX_DATA    128

typedef enum {
    BLE_TX_NONE = 0,
    BLE_TX_NUS,        /* 原有 NUS Notify */
    BLE_TX_NOTIFY,     /* 自定义 Notify 特征 */
    BLE_TX_INDICATE,   /* 自定义 Indicate 特征 */
} ble_tx_type_t;

struct tx_msg {
    uint8_t  data[TX_MSG_MAX_DATA];
    uint16_t len;
    ble_tx_type_t type;
};

#endif