#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble_handlers.h"
#include "zigbee_handlers.h"
#include "rs485_handlers.h"
#include <zephyr/fs/fs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
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


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE 2048
#define BT_RX_PRIORITY 5
#define BT_TX_PRIORITY 4
#define ZIGBEE_RX_PRIORITY 5
#define ZIGBEE_TX_PRIORITY 4
#define RS485_RX_PRIORITY  5
#define RS485_TX_PRIORITY  4

K_THREAD_STACK_DEFINE(bt_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(bt_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rs485_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rs485_tx_stack, THREAD_STACK_SIZE);

static struct k_thread bt_rx_thread_data;
static struct k_thread bt_tx_thread_data;
static struct k_thread zigbee_rx_thread_data;
static struct k_thread zigbee_tx_thread_data;
static struct k_thread rs485_rx_thread_data;
static struct k_thread rs485_tx_thread_data;

struct fs_littlefs lfs_data;

struct fs_mount_t mp = {
    .type = FS_LITTLEFS,
    .mnt_point = "/lfs",
    .storage_dev = (void *)PARTITION_ID(storage_partition),
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
                 (uintptr_t)PARTITION_ID(storage_partition),
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

int main(void) {
    int ret;
    bool is_ble_ok = false;
    bool is_zigbee_ok = false;

    ret = mount_log_fs();
    if (ret != 0) {
        LOG_ERR("FS unavailable, logs will NOT persist");
    }

    // lock_hfxo_permanently();

    ret = rs485_init();
    if (ret < 0) {
        LOG_ERR("RS485 init failed: %d", ret);
        return  -1;
    }

    printk("main: before ble_init\n");
    ret = ble_init();
    if (ret < 0) {
        LOG_ERR("BLE init failed: %d", ret);
        is_ble_ok = false;
    }else{
        is_ble_ok = true;
    }

    printk("main: after ble_init, err=%d\n", ret);
    ret = zigbee_init();
    if (ret < 0) {
        LOG_ERR("Zigbee init failed: %d", ret);
        is_zigbee_ok = false;
    }else{
        is_zigbee_ok = true;
    }

    ret = battery_monitor_init();
    if (ret < 0) {
        LOG_ERR("Battery monitor init failed: %d", ret);
    }
    battery_monitor_set_charge_state(false);

    LOG_INF("=== init done start thread ===");

    if (is_ble_ok) {
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
    }else{
        if( is_zigbee_ok){
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
        }else{
            LOG_ERR("No communication interface available, exiting.");
            return -1;
        }
    }

    k_thread_create(&rs485_rx_thread_data, rs485_rx_stack,
                    K_THREAD_STACK_SIZEOF(rs485_rx_stack),
                    rs485_rx_thread_entry, NULL, NULL, NULL,
                    RS485_RX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&rs485_rx_thread_data, "RS485_RX");

    k_thread_create(&rs485_tx_thread_data, rs485_tx_stack,
                    K_THREAD_STACK_SIZEOF(rs485_tx_stack),
                    rs485_tx_thread_entry, NULL, NULL, NULL,
                    RS485_TX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&rs485_tx_thread_data, "RS485_TX");

    if (thread_pool_init() != 0) {
        LOG_ERR("thread pool init failed");
        return -1;
    }

    // thread_pool_submit(rs485_test_thread, NULL, K_NO_WAIT);
    thread_pool_submit(timer_test_thread,  NULL, K_NO_WAIT);
    LOG_INF("All threads created");

    // battery_scan_timer(is_ble_ok, is_zigbee_ok);
    LOG_INF("All timers created");

    while (1) 
    {
        k_sleep(K_SECONDS(10));
    }
    return 0;
}