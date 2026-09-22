#include "battery_scan.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "message.h"
#include "ble_handlers.h"
#include "zigbee_handlers.h"
#include "battery_monitor.h"

LOG_MODULE_REGISTER(battery_scan, LOG_LEVEL_INF);

static struct k_timer scan_battery_timer;

static bool s_use_ble    = false;
static bool s_use_zigbee = false;

static void battery_scan_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    int voltage = 0, capacity = 0, level = 0;

    int rc = battery_monitor_get_voltage(&voltage);
    if (rc == 0) {
        rc = battery_monitor_get_capacity(&capacity);
    }
    if (rc != 0) {
        LOG_WRN("Battery read failed: %d", rc);
        return;
    }
    battery_monitor_get_level(&level);
    LOG_INF("Battery: %d mV, %d%%, %d/5 bars", voltage, capacity, level);

    struct k_msgq *tx_msgq = NULL;
    if (s_use_ble) {
        tx_msgq = get_ble_tx_msgq();
    } else if (s_use_zigbee) {
        tx_msgq = get_zigbee_tx_msgq();
    }
    if (tx_msgq == NULL) {
        LOG_ERR("No TX channel for battery report");
        return;
    }

    struct tx_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.data[0] = (uint8_t)(level & 0xFF);
    msg.len = 1;

    if (k_msgq_put(tx_msgq, &msg, K_NO_WAIT) != 0) {
        LOG_WRN("Battery Info TX queue full");
    } else {
        LOG_INF("Battery Info reply enqueued");
    }
}

int battery_scan_timer(bool use_ble, bool use_zigbee)
{
    s_use_ble    = use_ble;
    s_use_zigbee = use_zigbee;

    k_timer_init(&scan_battery_timer, battery_scan_timer_handler, NULL);
    k_timer_start(&scan_battery_timer, K_SECONDS(1), K_SECONDS(30));
    return 0;
}