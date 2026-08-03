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


K_MSGQ_DEFINE(rs485_rx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(rs485_tx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);


static void rs485_uart_cb(const struct device *dev, void *user_data) {
    ARG_UNUSED(user_data);

    uint8_t buf[64];
    int len;
    struct tx_msg msg;
    size_t total_len = 0;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            len = uart_fifo_read(dev, buf, sizeof(buf));
            if (len > 0) {
                if (total_len + len > TX_MSG_MAX_DATA) {
                    LOG_WRN("RS485 RX overflow");
                    break;
                }
                memcpy(msg.data + total_len, buf, len);
                total_len += len;
            }
        }
    }

    if (total_len > 0) {
        msg.len = total_len;
        if (k_msgq_put(&rs485_rx_msgq, &msg, K_NO_WAIT) != 0) {
            LOG_WRN("RS485 RX queue full");
        }
    }
}


void rs485_send(const uint8_t *data, size_t len) {
    if (!device_is_ready(uart485_dev) || !data || len == 0) {
        return;
    }
    gpio_pin_set_dt(&de_gpio, 1);           /* 发送模式 */
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart485_dev, data[i]);
    }
    uart_poll_out(uart485_dev, '\r');       /* 可选结束符 */
    uart_poll_out(uart485_dev, '\n');
    k_busy_wait(100);
    gpio_pin_set_dt(&de_gpio, 0);
}

void rs485_init(void) {
    if (!gpio_is_ready_dt(&de_gpio)) {
        LOG_ERR("DE GPIO not ready");
        return;
    }
    gpio_pin_configure_dt(&de_gpio, GPIO_OUTPUT);
    gpio_pin_set_dt(&de_gpio, 0);

    if (!device_is_ready(uart485_dev)) {
        LOG_ERR("UART485 not ready");
        return;
    }

    uart_irq_callback_user_data_set(uart485_dev, rs485_uart_cb, NULL);
    uart_irq_rx_enable(uart485_dev);
    LOG_INF("RS485 init done (RX interrupt enabled)");
}

void rs485_rx_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg rx_msg;

    while (1) {
        if (k_msgq_get(&rs485_rx_msgq, &rx_msg, K_FOREVER) != 0) {
            continue;
        }

        LOG_INF("RS485 RX %u bytes", rx_msg.len);
        LOG_HEXDUMP_INF(rx_msg.data, rx_msg.len, "RS485 Cmd:");

    }
}

void rs485_tx_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg msg;

    while (1) {
        if (k_msgq_get(&rs485_tx_msgq, &msg, K_FOREVER) != 0) {
            continue;
        }

        rs485_send(msg.data, msg.len);
        LOG_INF("RS485 sent %u bytes", msg.len);
        LOG_HEXDUMP_INF(msg.data, msg.len, "RS485 Sent:");
    }
}