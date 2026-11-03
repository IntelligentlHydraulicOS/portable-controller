#include "zigbee_handlers.h"
#include "worker.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "message.h"

LOG_MODULE_REGISTER(zigbee, LOG_LEVEL_INF);

#define UART_ZIGBEE_NODE DT_NODELABEL(uart22)
static const struct device *uart_zigbee_dev = DEVICE_DT_GET(UART_ZIGBEE_NODE);

K_MSGQ_DEFINE(zigbee_rx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(zigbee_tx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);

struct k_msgq *get_zigbee_tx_msgq(void)
{
    return &zigbee_tx_msgq;
}

static void zigbee_uart_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    uint8_t buf[64];
    int len;
    struct tx_msg msg;
    size_t total_len = 0;

    memset(&msg, 0, sizeof(msg));

    uart_irq_update(dev);
    while (uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            len = uart_fifo_read(dev, buf, sizeof(buf));
            if (len > 0) {
                if (total_len + len > TX_MSG_MAX_DATA) {
                    LOG_WRN("Zigbee RX overflow, discarding");
                    total_len = 0;
                    break;
                }
                memcpy(msg.data + total_len, buf, len);
                total_len += len;
            }
        }
    }

    if (total_len > 0) {
        msg.len = total_len;
        if (k_msgq_put(&zigbee_rx_msgq, &msg, K_NO_WAIT) != 0) {
            LOG_WRN("Zigbee RX queue full");
        }
    }
}

static void zigbee_send(const uint8_t *data, size_t len)
{
    if (!device_is_ready(uart_zigbee_dev) || !data || len == 0) {
        return;
    }
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart_zigbee_dev, data[i]);
    }
    LOG_INF("Zigbee sent %u bytes", len);
}

int zigbee_init(void)
{
    if (!device_is_ready(uart_zigbee_dev)) {
        LOG_ERR("UART_ZIGBEE not ready");
        return -ENODEV;
    }

    uart_irq_callback_user_data_set(uart_zigbee_dev, zigbee_uart_cb, NULL);
    uart_irq_rx_enable(uart_zigbee_dev);

    LOG_INF("Zigbee init done (DT-based)");
    return 0;
}

void zigbee_rx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    worker_t *worker = (worker_t *)p1;
    if (worker == NULL) {
        LOG_ERR("zigbee_rx_thread_entry: worker is NULL");
        return;
    }

    worker->source = CMD_SOURCE_ZIGBEE;
    worker->tx_msg = get_zigbee_tx_msgq();

    struct tx_msg rx_msg;
    uint8_t reply[TX_MSG_MAX_DATA];
    size_t reply_len = 0;

    while (1) {
        if (k_msgq_get(&zigbee_rx_msgq, &rx_msg, K_FOREVER) != 0) {
            continue;
        }

        LOG_INF("Zigbee RX %u bytes", rx_msg.len);
        LOG_HEXDUMP_INF(rx_msg.data, rx_msg.len, "Zigbee Cmd:");

        int ret = do_work(worker, rx_msg.data, rx_msg.len, reply, &reply_len);

        if (ret == 1 && reply_len > 0) {
            struct tx_msg tx_msg;
            memset(&tx_msg, 0, sizeof(tx_msg));
            memcpy(tx_msg.data, reply, reply_len);
            tx_msg.len = reply_len;
            if (k_msgq_put(worker->tx_msg, &tx_msg, K_NO_WAIT) != 0) {
                LOG_WRN("Zigbee TX queue full");
            }
        }
    }
}

void zigbee_tx_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg msg;

    while (1) {
        if (k_msgq_get(&zigbee_tx_msgq, &msg, K_FOREVER) != 0) {
            continue;
        }

        zigbee_send(msg.data, msg.len);
        LOG_INF("Zigbee sent %u bytes", msg.len);
        LOG_HEXDUMP_INF(msg.data, msg.len, "Zigbee Sent:");
    }
}