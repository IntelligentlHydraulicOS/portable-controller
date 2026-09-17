#include "rs485_handlers.h"
#include "worker.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "message.h"

LOG_MODULE_REGISTER(rs485, LOG_LEVEL_DBG);

#define UART485_NODE DT_NODELABEL(uart21)
#define RS485_DE_GPIO_CTRL_NODE DT_NODELABEL(gpio1)
#define RS485_DE_PIN            6

static const struct device *uart485_dev = DEVICE_DT_GET(UART485_NODE);
static const struct device *rs485_de_dev = DEVICE_DT_GET(RS485_DE_GPIO_CTRL_NODE);

static uint8_t rx_buf[TX_MSG_MAX_DATA];
static size_t rx_len = 0;

static uint8_t tx_buf[TX_MSG_MAX_DATA];
static size_t tx_len = 0;
static size_t tx_idx = 0;
static volatile bool tx_done = false;

static struct k_timer rs485_rx_timer;

K_MSGQ_DEFINE(rs485_rx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(rs485_tx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);

struct k_msgq *get_rs485_tx_msgq(void)
{
    return &rs485_tx_msgq;
}

static void rs485_rx_timeout_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    if (rx_len > 0) {
        struct tx_msg msg;
        msg.len = rx_len;
        memcpy(msg.data, rx_buf, rx_len);
        if (k_msgq_put(&rs485_rx_msgq, &msg, K_NO_WAIT) != 0) {
            LOG_WRN("RS485 RX queue full");
        }
        rx_len = 0;
    }
}

static void rs485_uart_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    uart_irq_update(dev);
    while (uart_irq_is_pending(dev)) {
        int err = uart_err_check(dev);
        if (err) {
            LOG_WRN("RS485 UART error: 0x%02X, discarding frame", err);
            rx_len = 0;
            k_timer_stop(&rs485_rx_timer);
            continue;
        }

        if (uart_irq_rx_ready(dev)) {
            uint8_t buf[64];
            int len = uart_fifo_read(dev, buf, sizeof(buf));
            if (len > 0) {
                if (rx_len + len > TX_MSG_MAX_DATA) {
                    LOG_WRN("RS485 RX overflow, discarding old frame");
                    rx_len = 0;
                }
                memcpy(rx_buf + rx_len, buf, len);
                rx_len += len;

                k_timer_start(&rs485_rx_timer, K_MSEC(5), K_NO_WAIT);
            }
        }

        if (uart_irq_tx_ready(dev)) {
            if (tx_idx < tx_len) {
                int sent = uart_fifo_fill(dev, &tx_buf[tx_idx], tx_len - tx_idx);
                tx_idx += sent;
            }
        }

        if (uart_irq_tx_complete(dev)) {
            uart_irq_tx_disable(dev);
            gpio_pin_set(rs485_de_dev, RS485_DE_PIN, 0); 
            tx_done = true;
            LOG_DBG("TX complete, DE pulled low");
        }
    }
}

void rs485_send(const uint8_t *data, size_t len)
{
    if (!device_is_ready(uart485_dev) || !data || len == 0) {
        return;
    }
    if (len > TX_MSG_MAX_DATA) {
        LOG_WRN("RS485 TX length truncated from %zu to %d", len, TX_MSG_MAX_DATA);
        len = TX_MSG_MAX_DATA;
    }

    memcpy(tx_buf, data, len);
    tx_len = len;
    tx_idx = 0;
    tx_done = false;

    gpio_pin_set(rs485_de_dev, RS485_DE_PIN, 1);
    uart_irq_tx_enable(uart485_dev);

    uint32_t timeout = 0;
    while (!tx_done && timeout < 1000) {
        k_sleep(K_MSEC(1));
        timeout++;
    }

    if (!tx_done) {
        LOG_ERR("RS485 TX timeout!");
        uart_irq_tx_disable(uart485_dev);
        gpio_pin_set(rs485_de_dev, RS485_DE_PIN, 0);
    }
}

int rs485_init(void)
{
    if (!device_is_ready(rs485_de_dev)) {
        LOG_ERR("RS485 DE GPIO controller not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure(rs485_de_dev, RS485_DE_PIN,
                                 GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("DE pin config failed: %d", ret);
        return ret;
    }

    if (!device_is_ready(uart485_dev)) {
        LOG_ERR("UART485 not ready");
        return -ENODEV;
    }

    k_timer_init(&rs485_rx_timer, rs485_rx_timeout_handler, NULL);

    rx_len = 0;
    tx_len = 0;
    tx_idx = 0;
    tx_done = false;

    uart_irq_callback_user_data_set(uart485_dev, rs485_uart_cb, NULL);
    uart_irq_rx_enable(uart485_dev);

    LOG_DBG("RS485 init done (interrupt driven, DE controlled by TX complete)");
    return 0;
}

void rs485_rx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg rx_msg;
    while (1) {
        if (k_msgq_get(&rs485_rx_msgq, &rx_msg, K_FOREVER) == 0) {
            LOG_DBG("RS485 RX %u bytes", rx_msg.len);
            LOG_HEXDUMP_INF(rx_msg.data, rx_msg.len, "RS485 Cmd:");
        }
    }
}

void rs485_tx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg msg;
    while (1) {
        if (k_msgq_get(&rs485_tx_msgq, &msg, K_FOREVER) == 0) {
            rs485_send(msg.data, msg.len);
            LOG_DBG("RS485 sent %u bytes", msg.len);
            LOG_HEXDUMP_INF(msg.data, msg.len, "RS485 Sent:");
        }
    }
}

/* 简单的 DE 引脚翻转测试函数，用于硬件验证 */
void test(void)
{
    while (1) {
        gpio_pin_set(rs485_de_dev, RS485_DE_PIN, 1);
        k_sleep(K_MSEC(1000));
        gpio_pin_set(rs485_de_dev, RS485_DE_PIN, 0);
        k_sleep(K_MSEC(1000));
    }
}

void rs485_test_thread(void *arg)
{
    struct tx_msg tx_msg;
    while (1) {
        memset(&tx_msg, 0, sizeof(tx_msg));
        
        // for (int i = 0; i < 255; i++) {
        //     tx_msg.data[i] = i;
        // }
        // tx_msg.len = 255;

        uint8_t test_data[] = {0x55, 0xAA, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x12};
        memcpy(tx_msg.data, test_data, sizeof(test_data));
        tx_msg.len = sizeof(test_data);

        if (k_msgq_put(&rs485_tx_msgq, &tx_msg, K_NO_WAIT) != 0) {
            LOG_WRN("TX queue full");
        } else {
            LOG_DBG("reply enqueued");
        }
        k_sleep(K_SECONDS(5));
    }
}