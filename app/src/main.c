/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 功能：蓝牙 NUS 接收数据 -> 处理后通过 RS-485 发送
 * 硬件连接：
 *   - RS-485 TX: P1.04
 *   - RS-485 RX: P1.05
 *   - RS-485 DE: P1.06 (高电平发送，低电平接收)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* ---------- 硬件定义 ---------- */
#define UART_NODE DT_NODELABEL(uart21)
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

#define DE_NODE DT_NODELABEL(de)
static const struct gpio_dt_spec de_gpio = GPIO_DT_SPEC_GET(DE_NODE, gpios);

/* ---------- 蓝牙广播数据 ---------- */
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

/* ---------- 本地函数：RS-485 发送（带方向控制） ---------- */
static void rs485_send(const uint8_t *data, size_t len) {
    if (!device_is_ready(uart_dev) || !data || len == 0) {
        return;
    }

    /* 使能发送（DE 拉高） */
    gpio_pin_set_dt(&de_gpio, 1);

    /* 发送数据（轮询方式） */
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart_dev, data[i]);
    }
    /* 发送结束符（换行） */
    uart_poll_out(uart_dev, '\r');
    uart_poll_out(uart_dev, '\n');

    /* 等待最后一个字节发送完成（硬件延时） */
    k_busy_wait(100);  // 115200bps 下约 100us 足够

    /* 切换回接收模式（DE 拉低） */
    gpio_pin_set_dt(&de_gpio, 0);
}

/* ---------- 数据处理函数 ---------- */
static void process_data_and_send(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return;
    }

    #define PREFIX "BLE>"
    #define MAX_BUF_SIZE 256

    uint8_t buf[MAX_BUF_SIZE];
    size_t total_len = 0;

    /* 添加前缀 */
    size_t prefix_len = strlen(PREFIX);
    if (prefix_len + len + 2 > MAX_BUF_SIZE) {
        LOG_ERR("Buffer too small, sending raw data");
        rs485_send(data, len);
        return;
    }

    memcpy(buf, PREFIX, prefix_len);
    total_len += prefix_len;

    memcpy(buf + total_len, data, len);
    total_len += len;

    rs485_send(buf, total_len);
    LOG_INF("Processed and sent %u bytes via RS-485", total_len);
}

/* ---------- 蓝牙 NUS 回调 ---------- */
static void nus_notif_enabled(bool enabled, void *ctx) {
    ARG_UNUSED(ctx);
    LOG_INF("NUS notifications %s", enabled ? "Enabled" : "Disabled");
}

static void nus_received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx) {
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);

    if (!data || len == 0) {
        return;
    }

    LOG_INF("BLE received %u bytes", len);
    LOG_HEXDUMP_INF(data, len, "Raw data:");

    process_data_and_send((const uint8_t *)data, len);
}

/* ---------- NUS 回调结构体 ---------- */
static struct bt_nus_cb nus_listener = {
    .notif_enabled = nus_notif_enabled,
    .received = nus_received,
};

/* ---------- 主函数 ---------- */
int main(void) {
    int err;

    LOG_INF("=== BLE to RS-485 Gateway ===");
    LOG_INF("TX: P1.04, RX: P1.05, DE: P1.06");

    /* 1. 初始化 RS-485 方向控制 GPIO */
    if (!gpio_is_ready_dt(&de_gpio)) {
        LOG_ERR("DE GPIO not ready");
        return -1;
    }
    gpio_pin_configure_dt(&de_gpio, GPIO_OUTPUT);
    gpio_pin_set_dt(&de_gpio, 0);  // 默认接收模式
    LOG_INF("DE GPIO initialized");

    /* 2. 检查 UART */
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -1;
    }
    LOG_INF("UART ready");

    /* 3. 注册 NUS 回调（先于 bt_enable） */
    err = bt_nus_cb_register(&nus_listener, NULL);
    if (err) {
        LOG_ERR("Failed to register NUS callback: %d", err);
        return -1;
    }
    LOG_INF("NUS callback registered");

    /* 4. 初始化蓝牙 */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return -1;
    }
    LOG_INF("Bluetooth initialized");

    /* 5. 开始广播 */
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed (err %d)", err);
        return -1;
    }
    LOG_INF("Advertising as %s", CONFIG_BT_DEVICE_NAME);

    /* 主循环空闲 - 不发送任何数据，只等待接收 */
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    return 0;
}