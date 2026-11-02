#include "battery_scan.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "message.h"
#include "ble_handlers.h"
#include "zigbee_handlers.h"
#include "battery_monitor.h"
#include "gpio.h"

LOG_MODULE_REGISTER(battery_scan, LOG_LEVEL_INF);

static struct k_timer scan_battery_timer;
static K_SEM_DEFINE(adc_work_sem, 0, 1);

static struct k_work adc_sample_work;

static bool s_use_ble    = false;
static bool s_use_zigbee = false;

static void battery_scan_timer_handler(struct k_timer *timer)
{
    k_work_submit(&adc_sample_work);
}

static void adc_sample_work_handler(struct k_work *work)
{
    
    int voltage = 0, capacity = 0, level = 0;
    int32_t current = 0;
    int32_t adout_voltage = 0;

    int rc = battery_monitor_get_voltage(&voltage);
    if (rc != 0) {
        LOG_WRN("Battery voltage read failed: %d", rc);
        return;
    }

    battery_monitor_calc_capacity(voltage, &capacity);
    
    level = capacity / 20;
    if (level > 5) level = 5;

    LOG_INF("Battery: %d mV, %d%%, %d/5 bars", voltage, capacity, level);

    k_msleep(5);

    rc = battery_monitor_get_current(&current);
    if (rc == 0) {
        LOG_INF("Current: %d mA", current);
    } else {
        LOG_WRN("Current read failed: %d", rc);
    }

    //TODO if (current < 0) do something to indicate discharging, else charging;
    adout_voltage = read_gpio_adout();
    if (adout_voltage >= 0) {
        LOG_INF("ADOUT Voltage: %d V", adout_voltage);
    } else {
        LOG_WRN("ADOUT Voltage read failed: %d", adout_voltage);
    }

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
    msg.type = BLE_TX_NOTIFY;
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
    k_work_init(&adc_sample_work, adc_sample_work_handler);
    k_timer_init(&scan_battery_timer, battery_scan_timer_handler, NULL);
    k_timer_start(&scan_battery_timer, K_SECONDS(5), K_SECONDS(30));

    return 0;
}