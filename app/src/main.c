#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble_handlers.h"
#include "zigbee_handlers.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE 2048
#define BT_RX_PRIORITY 5
#define BT_TX_PRIORITY 4
#define ZIGBEE_RX_PRIORITY 5
#define ZIGBEE_TX_PRIORITY 4

K_THREAD_STACK_DEFINE(bt_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(bt_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_tx_stack, THREAD_STACK_SIZE);

static struct k_thread bt_rx_thread_data;
static struct k_thread bt_tx_thread_data;
static struct k_thread zigbee_rx_thread_data;
static struct k_thread zigbee_tx_thread_data;

int main(void) {
    LOG_INF("=== Gateway Started ===");

    zigbee_init();

    ble_init();

    k_thread_create(&bt_rx_thread_data, bt_rx_stack,
                    K_THREAD_STACK_SIZEOF(bt_rx_stack),
                    ble_rx_thread_entry, NULL, NULL, NULL,
                    BT_RX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&bt_rx_thread_data, "BT_RX");

    k_thread_create(&bt_tx_thread_data, bt_tx_stack,
                    K_THREAD_STACK_SIZEOF(bt_tx_stack),
                    ble_tx_thread_entry, NULL, NULL, NULL,
                    BT_TX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&bt_tx_thread_data, "BT_TX");

    k_thread_create(&zigbee_rx_thread_data, zigbee_rx_stack,
                    K_THREAD_STACK_SIZEOF(zigbee_rx_stack),
                    zigbee_rx_thread_entry, NULL, NULL, NULL,
                    ZIGBEE_RX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&zigbee_rx_thread_data, "ZIG_RX");

    k_thread_create(&zigbee_tx_thread_data, zigbee_tx_stack,
                    K_THREAD_STACK_SIZEOF(zigbee_tx_stack),
                    zigbee_tx_thread_entry, NULL, NULL, NULL,
                    ZIGBEE_TX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&zigbee_tx_thread_data, "ZIG_TX");

    LOG_INF("All threads created");

    while (1) {
        k_sleep(K_SECONDS(1));
    }
    return 0;
}