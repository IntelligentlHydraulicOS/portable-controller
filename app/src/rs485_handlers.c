#include "rs485_handlers.h"
#include "worker.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "message.h"

LOG_MODULE_REGISTER(rs485, LOG_LEVEL_INF);

#define UART485_NODE DT_NODELABEL(uart21)
#define DE_NODE DT_NODELABEL(de)

static const struct device *uart485_dev = DEVICE_DT_GET(UART485_NODE);
static const struct gpio_dt_spec de_gpio = GPIO_DT_SPEC_GET(DE_NODE, gpios);

/* 接收缓冲区（静态，跨中断保持） */
static uint8_t rx_buf[TX_MSG_MAX_DATA];
static size_t rx_len = 0;

K_MSGQ_DEFINE(rs485_rx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(rs485_tx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);

struct k_msgq *get_rs485_tx_msgq(void)
{
    return &rs485_tx_msgq;
}

/* ---------- 中断接收回调 ---------- */
static void rs485_uart_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    uint8_t buf[64];
    int len;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            len = uart_fifo_read(dev, buf, sizeof(buf));
            if (len > 0) {
                if (rx_len + len > TX_MSG_MAX_DATA) {
                    LOG_WRN("RS485 RX overflow, discarding old frame");
                    rx_len = 0;  // 溢出则丢弃当前帧
                }
                memcpy(rx_buf + rx_len, buf, len);
                rx_len += len;
            }
        }
    }

    /* 检查是否收到结束标志（'\r' 或 '\n'），也可以添加超时机制 */
    // if (rx_len > 0 && (rx_buf[rx_len-1] == '\n' || rx_buf[rx_len-1] == '\r')) {
    if (rx_len > 0) {
        struct tx_msg msg;
        msg.len = rx_len;
        memcpy(msg.data, rx_buf, rx_len);
        if (k_msgq_put(&rs485_rx_msgq, &msg, K_NO_WAIT) != 0) {
            LOG_WRN("RS485 RX queue full");
        }
        rx_len = 0;  // 清空缓冲区，准备接收下一帧
    }
}

/* ---------- 发送函数（轮询 + 精确等待） ---------- */
void rs485_send(const uint8_t *data, size_t len)
{
    if (!device_is_ready(uart485_dev) || !data || len == 0) {
        return;
    }

    // gpio_pin_set_dt(&de_gpio, 1);          /* 发送模式 */

    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart485_dev, data[i]);
    }
    /* 可选：如果你需要添加结束符，就保留；否则去掉下面两行 */
    // uart_poll_out(uart485_dev, '\r');
    // uart_poll_out(uart485_dev, '\n');

    /* 动态等待：确保所有字节（含结束符）均已发出 */
    uint32_t total_bytes = len; /* 如果加了结束符，需要 +2 */
    uint32_t byte_time_us = (10 * 1000000) / 115200;  // ≈86.8us
    uint32_t wait_us = total_bytes * byte_time_us + 100; // 增加安全余量
    k_busy_wait(wait_us);

    // gpio_pin_set_dt(&de_gpio, 0);           /* 切回接收 */
}

/* ---------- 初始化 ---------- */
int rs485_init(void)
{
    if (!gpio_is_ready_dt(&de_gpio)) {
        LOG_ERR("DE GPIO not ready");
        return -ENODEV;
    }
    int ret = gpio_pin_configure_dt(&de_gpio, GPIO_OUTPUT|GPIO_PULL_UP);
    if (ret < 0) {
        LOG_ERR("DE config failed: %d", ret);
        return ret;
    }
    ret = gpio_pin_set_dt(&de_gpio, 1);
    if (ret < 0) {
        LOG_ERR("DE set high failed: %d", ret);
        return ret;
    }
    if (!device_is_ready(uart485_dev)) {
        LOG_ERR("UART485 not ready");
        return -ENODEV;
    }

    rx_len = 0;
    uart_irq_callback_user_data_set(uart485_dev, rs485_uart_cb, NULL);
    uart_irq_rx_enable(uart485_dev);
    LOG_INF("RS485 init done (RX interrupt, polling TX)");
    return 0;
}

/* ---------- 接收线程 ---------- */
void rs485_rx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg rx_msg;
    while (1) {
        if (k_msgq_get(&rs485_rx_msgq, &rx_msg, K_FOREVER) == 0) {
            LOG_INF("RS485 RX %u bytes", rx_msg.len);
            LOG_HEXDUMP_INF(rx_msg.data, rx_msg.len, "RS485 Cmd:");
        }
    }
}

/* ---------- 发送线程 ---------- */
void rs485_tx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg msg;
    while (1) {
        if (k_msgq_get(&rs485_tx_msgq, &msg, K_FOREVER) == 0) {
            rs485_send(msg.data, msg.len);
            LOG_INF("RS485 sent %u bytes", msg.len);
            LOG_HEXDUMP_INF(msg.data, msg.len, "RS485 Sent:");
        }
    }
}

void test(void)
{
    while (1) {
        gpio_pin_set_dt(&de_gpio, 1);
        k_sleep(K_MSEC(1000));
        gpio_pin_set_dt(&de_gpio, 0);
        k_sleep(K_MSEC(1000));
    }
}

/* ---------- 测试线程（每5秒发送一帧） ---------- */
void rs485_test_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    // test();
    struct tx_msg tx_msg;
    while (1) {
        memset(&tx_msg, 0, sizeof(tx_msg));
        uint8_t test_data[] = {0x55, 0xAA, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x12};
        memcpy(tx_msg.data, test_data, sizeof(test_data));
        tx_msg.len = sizeof(test_data);
        if (k_msgq_put(&rs485_tx_msgq, &tx_msg, K_NO_WAIT) != 0) {
            LOG_WRN("TX queue full");
        } else {
            LOG_INF("reply enqueued");
        }
        k_sleep(K_SECONDS(5));
    }
}