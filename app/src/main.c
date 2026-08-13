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

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define THREAD_STACK_SIZE 2048
#define BT_RX_PRIORITY 5
#define BT_TX_PRIORITY 4
#define ZIGBEE_RX_PRIORITY 5
#define ZIGBEE_TX_PRIORITY 4
#define RS485_RX_PRIORITY  5
#define RS485_TX_PRIORITY  4

#define STORAGE_PARTITION_NODE DT_NODELABEL(storage_partition)

/* 获取 Flash 设备（自动关联到 CPUAPP_RRAM） */
#define FLASH_DEV_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_PARTITION_NODE)

/* 获取分区偏移和大小（直接从设备树） */
#define STORAGE_OFFSET DT_REG_ADDR(STORAGE_PARTITION_NODE)
#define STORAGE_SIZE   DT_REG_SIZE(STORAGE_PARTITION_NODE)

K_THREAD_STACK_DEFINE(bt_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(bt_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(zigbee_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rs485_rx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rs485_tx_stack, THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(rs485_test_stack, THREAD_STACK_SIZE);

static struct k_thread bt_rx_thread_data;
static struct k_thread bt_tx_thread_data;
static struct k_thread zigbee_rx_thread_data;
static struct k_thread zigbee_tx_thread_data;
static struct k_thread rs485_rx_thread_data;
static struct k_thread rs485_tx_thread_data;
static struct k_thread rs485_test_thread_data;

static const struct device *flash_dev = DEVICE_DT_GET(FLASH_DEV_NODE);
int mount_log_fs(void) {
    if (!device_is_ready(flash_dev)) {
        LOG_ERR("Flash device not ready");
        return -ENODEV;
    }

    /* 构造 flash_area */
    struct flash_area fa = {
        .fa_dev = flash_dev,
        .fa_off = STORAGE_OFFSET,
        .fa_size = STORAGE_SIZE,
    };

    struct fs_mount_t mp = {
        .type = FS_LITTLEFS,
        .mnt_point = "/lfs",
        .storage_dev = &fa,
        .fs_data = NULL,
        .flags = 0,
    };

    int rc = fs_mount(&mp);
    if (rc == 0) {
        LOG_INF("LittleFS mounted on /lfs");
        return 0;
    }

    LOG_ERR("Mount failed (err %d), attempting to format...", rc);

    /* 格式化分区 */
    rc = fs_mkfs(FS_LITTLEFS, (uintptr_t)&fa, NULL, 0);
    if (rc != 0) {
        LOG_ERR("Format failed (err %d)", rc);
        return rc;
    }

    rc = fs_mount(&mp);
    if (rc == 0) {
        LOG_INF("LittleFS mounted after format");
    } else {
        LOG_ERR("Re-mount after format failed (err %d)", rc);
    }
    return rc;
}

int main(void) {
    // int ret = mount_log_fs();
    // if (ret == 0) {
    //     LOG_INF("Log filesystem ready, logs will be saved.");
    // } else {
    //     LOG_WRN("Log filesystem unavailable, logs will NOT persist.");
    // }

    LOG_INF("=== Gateway Started ===");

    // zigbee_init();
    rs485_init();
    ble_init();

    LOG_INF("=== init done start thread ===");

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

    k_thread_create(&rs485_test_thread_data, rs485_test_stack,
                    K_THREAD_STACK_SIZEOF(rs485_test_stack),
                    rs485_test_thread, NULL, NULL, NULL,
                    RS485_TX_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&rs485_test_thread_data, "RS485_TEST");

    
    LOG_INF("All threads created");

    while (1) {
        k_sleep(K_SECONDS(1));
    }
    return 0;
}