#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble_handlers.h"
#include "zigbee_handlers.h"
#include "rs485_handlers.h"
#include <zephyr/fs/fs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include "battery_monitor.h"
#include "message.h"
#include "timer.h"
#include "worker.h"
#include "thread_pool.h"
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/sys/notify.h>
#include "battery_scan.h"
#include "proto_config.h"
#include "led_strip.h"
#include "led_control.h"
#include "nfc_handlers.h"
#include "gpio.h"
#include "watchdog.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE 2048
#define BT_RX_PRIORITY 5
#define BT_TX_PRIORITY 4
#define ZIGBEE_RX_PRIORITY 5
#define ZIGBEE_TX_PRIORITY 4
#define RS485_RX_PRIORITY  5
#define RS485_TX_PRIORITY  4

#define LED_THREAD_STACK_SIZE  1024
#define LED_THREAD_PRIORITY    7
#define NFC_THREAD_PRIORITY    6    

K_THREAD_STACK_DEFINE(bt_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(bt_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rs485_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rs485_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(led_stack, LED_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(nfc_stack, THREAD_STACK_SIZE);

static struct k_thread led_thread_data;
static struct k_thread bt_rx_thread_data;
static struct k_thread bt_tx_thread_data;
static struct k_thread zigbee_rx_thread_data;
static struct k_thread zigbee_tx_thread_data;
static struct k_thread rs485_rx_thread_data;
static struct k_thread rs485_tx_thread_data;
static struct k_thread nfc_thread_data;

static protocol_t current_proto = PROTO_NONE;

struct fs_littlefs lfs_data;

struct fs_mount_t mp = {
    .type = FS_LITTLEFS,
    .mnt_point = "/lfs",
    .storage_dev = (void *)PARTITION_ID(littlefs_storage_partition),
    .fs_data = &lfs_data,
    .flags = 0,
};

int mount_log_fs(void)
{
    int rc = fs_mount(&mp);
    if (rc == 0) {
        LOG_INF("LittleFS mounted on /lfs");
        return 0;
    }
    if (rc == -EBUSY) {
        /* 日志后端已提前挂载，属正常 */
        LOG_INF("LittleFS already mounted");
        return 0;
    }

    LOG_WRN("Mount failed (%d), formatting...", rc);
    rc = fs_mkfs(FS_LITTLEFS,
                 (uintptr_t)PARTITION_ID(littlefs_storage_partition),
                 NULL, 0);
    if (rc != 0) {
        LOG_ERR("Format failed (%d)", rc);
        return rc;
    }

    rc = fs_mount(&mp);
    if (rc == 0) {
        LOG_INF("LittleFS mounted after format");
    } else {
        LOG_ERR("Re-mount failed (%d)", rc);
    }
    return rc;
}

static protocol_t decide_protocol(bool ble_enabled, bool zigbee_enabled)
{
    if (ble_enabled) {
        return PROTO_BLE;
    }else {
        if (zigbee_enabled) {
            return PROTO_ZIGBEE;
        }
    }

    return PROTO_NONE;
}

int main(void) {
    int ret;

    ret = mount_log_fs();
    if (ret != 0) {
        LOG_ERR("FS unavailable, logs will NOT persist");
    }else{
        LOG_INF("FS mounted, logs will persist");
    }
    ret = init_gpio();
    if (ret < 0) {
        LOG_ERR("GPIO init failed: %d", ret);
        return  -1;
    }else{
        LOG_INF("GPIO initialized");
    }

    ret = rs485_init();
    if (ret < 0) {
        LOG_ERR("RS485 init failed: %d", ret);
        return  -1;
    }else{
        LOG_INF("RS485 initialized");
    }

    ret = battery_monitor_init();
    if (ret < 0) {
        LOG_ERR("Battery monitor init failed: %d", ret);
    }else{
        LOG_INF("Battery monitor initialized");
    }

    // init Settings for NFC and BLE
    ret = settings_subsys_init();
    if (ret) {
        LOG_ERR("Settings init failed: %d", ret);
    }

    bool ble_enabled, zigbee_enabled;
    proto_config_load(&ble_enabled, &zigbee_enabled);
    LOG_INF("Protocol config: BLE=%d Zigbee=%d", ble_enabled, zigbee_enabled);

    current_proto = decide_protocol(ble_enabled, zigbee_enabled);
    switch (current_proto) {
    case PROTO_BLE:
        LOG_INF("Selected: BLE");
        disable_zigbee();
        ret = ble_init();
        if (ret < 0) {
            LOG_ERR("BLE init failed: %d", ret);
            return -1;
        }
        break;

    case PROTO_ZIGBEE:
        LOG_INF("Selected: Zigbee");
        enable_zigbee();
        ret = zigbee_init();
        if (ret < 0) {
            LOG_ERR("Zigbee init failed: %d", ret);
            return -1;
        }
        break;

    case PROTO_NONE:
    default:
        LOG_ERR("No protocol enabled (ble=%d zigbee=%d)",
                ble_enabled, zigbee_enabled);
        return -1;
    }

    LOG_INF("=== init done, start thread ===");

    worker_t *worker = get_worker();
    if (ble_enabled) {
        k_thread_create(&bt_rx_thread_data, bt_rx_stack,
                        K_THREAD_STACK_SIZEOF(bt_rx_stack),
                        ble_rx_thread_entry, worker, NULL, NULL,
                        BT_RX_PRIORITY, 0, K_NO_WAIT);
        k_thread_name_set(&bt_rx_thread_data, "BT_RX");

        k_thread_create(&bt_tx_thread_data, bt_tx_stack,
                        K_THREAD_STACK_SIZEOF(bt_tx_stack),
                        ble_tx_thread_entry, worker, NULL, NULL,
                        BT_TX_PRIORITY, 0, K_NO_WAIT);
        k_thread_name_set(&bt_tx_thread_data, "BT_TX");
    }else{
        if( zigbee_enabled){
                         k_thread_create(&zigbee_rx_thread_data, zigbee_rx_stack,
                            K_THREAD_STACK_SIZEOF(zigbee_rx_stack),
                            zigbee_rx_thread_entry, worker, NULL, NULL,
                            ZIGBEE_RX_PRIORITY, 0, K_NO_WAIT);
            k_thread_name_set(&zigbee_rx_thread_data, "ZIG_RX");

            k_thread_create(&zigbee_tx_thread_data, zigbee_tx_stack,
                            K_THREAD_STACK_SIZEOF(zigbee_tx_stack),
                            zigbee_tx_thread_entry, worker, NULL, NULL,
                            ZIGBEE_TX_PRIORITY, 0, K_NO_WAIT);
            k_thread_name_set(&zigbee_tx_thread_data, "ZIG_TX");
        }else{
            LOG_ERR("No communication interface available, exiting.");
            return -1;
        }
    }

    k_thread_create(&rs485_rx_thread_data, rs485_rx_stack,
                    K_THREAD_STACK_SIZEOF(rs485_rx_stack),
                    rs485_rx_thread_entry, worker, NULL, NULL,
                    RS485_RX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&rs485_rx_thread_data, "RS485_RX");

    k_thread_create(&rs485_tx_thread_data, rs485_tx_stack,
                    K_THREAD_STACK_SIZEOF(rs485_tx_stack),
                    rs485_tx_thread_entry, worker, NULL, NULL,
                    RS485_TX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&rs485_tx_thread_data, "RS485_TX");

    k_thread_create(&led_thread_data, led_stack,
                    K_THREAD_STACK_SIZEOF(led_stack),
                    led_thread_entry, NULL, NULL, NULL,
                    LED_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&led_thread_data, "LED_CTL");

    k_thread_create(&nfc_thread_data, nfc_stack,
                    K_THREAD_STACK_SIZEOF(nfc_stack),
                    nfc_thread_entry, worker, NULL, NULL,
                    NFC_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&nfc_thread_data, "NFC_TASK");
    LOG_INF("NFC thread created");

    LOG_INF("LED control thread started");
    if (thread_pool_init() != 0) {
        LOG_ERR("thread pool init failed");
        return -1;
    }

    thread_pool_submit(feeddog_thread, NULL, K_NO_WAIT);
    // thread_pool_submit(rs485_test_thread, NULL, K_NO_WAIT);
    // thread_pool_submit(timer_test_thread,  NULL, K_NO_WAIT);
    // thread_pool_submit(led_test_thread,  NULL, K_NO_WAIT);
    // thread_pool_submit(led_control_test_thread,  NULL, K_NO_WAIT);

    LOG_INF("All threads created");

    battery_scan_timer(ble_enabled, zigbee_enabled);
    LOG_INF("All timers created");

    running_led(LED_ON);
    link_led(LED_OFF);
    enout_on();
    enlock_on();
    //TODO: set battery state based on GPIO or charger detection
    battery_monitor_set_charge_state(BAT_STATE_DISCHARGING);

    while (1) 
    {
        k_sleep(K_SECONDS(5));
    }

    //unreachable code, but if we ever exit the main thread, clean up the worker
    free_worker(worker);
    return 0;
}