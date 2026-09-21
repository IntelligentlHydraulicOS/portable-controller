#include "proto_config.h"
#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(proto_cfg, LOG_LEVEL_DBG);

/* 文件格式：2 字节 [ble_enabled][zigbee_enabled] */
#define CFG_FILE_SIZE 2

/* 默认值：BLE 启用，Zigbee 禁用（可按需改成 true/false） */
#define DEFAULT_BLE_ENABLED    true
#define DEFAULT_ZIGBEE_ENABLED false

int proto_config_load(bool *ble_enabled, bool *zigbee_enabled)
{
    if (!ble_enabled || !zigbee_enabled) {
        return -EINVAL;
    }

    *ble_enabled    = DEFAULT_BLE_ENABLED;
    *zigbee_enabled = DEFAULT_ZIGBEE_ENABLED;

    struct fs_file_t file;
    fs_file_t_init(&file);

    int rc = fs_open(&file, PROTO_CFG_PATH, FS_O_READ);
    if (rc < 0) {
        LOG_WRN("proto.cfg not found (%d), using defaults: ble=%d zigbee=%d",
                rc, *ble_enabled, *zigbee_enabled);
        return rc;
    }

    uint8_t buf[CFG_FILE_SIZE] = {0};
    ssize_t n = fs_read(&file, buf, sizeof(buf));
    fs_close(&file);

    if (n != CFG_FILE_SIZE) {
        LOG_WRN("proto.cfg size invalid (%d bytes), using defaults", (int)n);
        return -EINVAL;
    }

    /* 只有 0/1 视为有效，其他值按禁用处理 */
    *ble_enabled    = (buf[0] == 1);
    *zigbee_enabled = (buf[1] == 1);

    LOG_INF("proto.cfg loaded: ble=%d zigbee=%d",
            *ble_enabled, *zigbee_enabled);
    return 0;
}

int proto_config_save(bool ble_enabled, bool zigbee_enabled)
{
    struct fs_file_t file;
    fs_file_t_init(&file);

    int rc = fs_open(&file, PROTO_CFG_PATH,
                     FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (rc < 0) {
        LOG_ERR("open %s for write failed: %d", PROTO_CFG_PATH, rc);
        return rc;
    }

    uint8_t buf[CFG_FILE_SIZE] = {
        ble_enabled    ? 1 : 0,
        zigbee_enabled ? 1 : 0,
    };

    ssize_t n = fs_write(&file, buf, sizeof(buf));
    fs_close(&file);

    if (n != CFG_FILE_SIZE) {
        LOG_ERR("write proto.cfg failed: %d", (int)n);
        return -EIO;
    }

    LOG_INF("proto.cfg saved: ble=%d zigbee=%d", ble_enabled, zigbee_enabled);
    return 0;
}