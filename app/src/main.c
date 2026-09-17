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

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

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
K_THREAD_STACK_DEFINE(rs485_test_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(timer_test_stack, THREAD_STACK_SIZE);


static struct k_thread bt_rx_thread_data;
static struct k_thread bt_tx_thread_data;
static struct k_thread zigbee_rx_thread_data;
static struct k_thread zigbee_tx_thread_data;
static struct k_thread rs485_rx_thread_data;
static struct k_thread rs485_tx_thread_data;
static struct k_thread rs485_test_thread_data;
static struct k_thread timer_test_thread_data;


#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/sys/notify.h>

static struct onoff_client hfxo_cli;

static int lock_hfxo_permanently(void)
{
    struct onoff_manager *clk_mgr;
    int ret;

    clk_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
    if (!clk_mgr) {
        LOG_ERR("Unable to get HF clock manager");
        return -ENXIO;
    }

    sys_notify_init_spinwait(&hfxo_cli.notify);
    ret = onoff_request(clk_mgr, &hfxo_cli);
    if (ret < 0) {
        LOG_ERR("HFXO request failed: %d", ret);
        return ret;
    }

    while (sys_notify_fetch_result(&hfxo_cli.notify, &ret) != 0) {
        k_busy_wait(100);
    }
    if (ret < 0) {
        LOG_ERR("HFXO start failed: %d", ret);
        return ret;
    }

    NRF_CLOCK->TASKS_XOSTART = 1;
    uint32_t timeout = 10000;
    while (NRF_CLOCK->EVENTS_XOSTARTED == 0 && timeout--) {
        k_busy_wait(1);
    }
    if (NRF_CLOCK->EVENTS_XOSTARTED == 0) {
        LOG_ERR("HFXO start timeout");
        return -ETIMEDOUT;
    }
    NRF_CLOCK->EVENTS_XOSTARTED = 0;

    LOG_INF("HFXO locked permanently");
    return 0;
}

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

    LOG_INF("=== Gateway Started ===");
    lock_hfxo_permanently();
    ret = zigbee_init();
    if (ret < 0) {
        LOG_ERR("Zigbee init failed: %d", ret);
        is_zigbee_ok = false;
    }else{
        is_zigbee_ok = true;
    }

    ret = rs485_init();
    if (ret < 0) {
        LOG_ERR("RS485 init failed: %d", ret);
        return  -1;
    }

    ret = ble_init();
    if (ret < 0) {
        LOG_ERR("BLE init failed: %d", ret);
        is_ble_ok = false;
    }else{
        is_ble_ok = true;
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

    // k_thread_create(&rs485_test_thread_data, rs485_test_stack,
    //                 K_THREAD_STACK_SIZEOF(rs485_test_stack),
    //                 rs485_test_thread, NULL, NULL, NULL,
    //                 RS485_TX_PRIORITY, 0, K_NO_WAIT);
    // k_thread_name_set(&rs485_test_thread_data, "RS485_TEST");

    k_thread_create(&timer_test_thread_data, timer_test_stack,
                    K_THREAD_STACK_SIZEOF(timer_test_stack),
                    timer_test_thread, NULL, NULL, NULL,
                    RS485_TX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&timer_test_thread_data, "TIMER_TEST");

    LOG_INF("All threads created");

    struct k_msgq *tx_msgq = NULL;
    struct tx_msg msg;
    while (1) {
        int voltage, capacity, level;
        ret = battery_monitor_get_voltage(&voltage);
        if (ret == 0) {
            ret = battery_monitor_get_capacity(&capacity);
            if (ret == 0) {
                battery_monitor_get_level(&level);
                LOG_INF("Battery: %d mV, %d%%, %d/5 bars", voltage, capacity, level);
                if(is_ble_ok){
                    tx_msgq = get_ble_tx_msgq();
                }else if(!is_ble_ok && is_zigbee_ok){
                    tx_msgq = get_zigbee_tx_msgq();
                }
                memset(&msg, 0, sizeof(struct tx_msg));
                msg.data[0] = (uint8_t)(level & 0xFF);
                msg.len = 1;
                if (k_msgq_put(tx_msgq, &msg, K_NO_WAIT) != 0) {
                    LOG_WRN("Battery Info TX queue full");
                } else {
                    LOG_INF("Battery Info reply enqueued");
                }
            }
        }
        k_sleep(K_SECONDS(10));
    }
    return 0;
}