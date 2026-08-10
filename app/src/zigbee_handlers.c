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
#define LINK_ZIG_NODE DT_NODELABEL(link_zig)
#define MODE_ZIG_NODE DT_NODELABEL(mode_zig)

static const struct device *uart_zigbee_dev = DEVICE_DT_GET(UART_ZIGBEE_NODE);
static const struct gpio_dt_spec link_zig_gpio = GPIO_DT_SPEC_GET(LINK_ZIG_NODE, gpios);
static const struct gpio_dt_spec mode_zig_gpio = GPIO_DT_SPEC_GET(MODE_ZIG_NODE, gpios);


K_MSGQ_DEFINE(zigbee_rx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(zigbee_tx_msgq, sizeof(struct tx_msg), TX_MSG_QUEUE_SIZE, 4);

struct k_msgq *get_zigbee_tx_msgq(void) {
    return &zigbee_tx_msgq;
}

static void zigbee_uart_cb(const struct device *dev, void *user_data) {
    ARG_UNUSED(user_data);

    uint8_t buf[64];
    int len;
    struct tx_msg msg;
    size_t total_len = 0;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            len = uart_fifo_read(dev, buf, 64);
            if (len > 0) {
                if (total_len + len > TX_MSG_MAX_DATA) {
                    LOG_WRN("Zigbee RX overflow");
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

static void zigbee_send(const uint8_t *data, size_t len) {
    if (!device_is_ready(uart_zigbee_dev) || !data || len == 0) {
        return;
    }
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart_zigbee_dev, data[i]);
    }
    LOG_INF("Zigbee sent %u bytes", len);
}

void zigbee_init(void) {
    if (!gpio_is_ready_dt(&link_zig_gpio)) {
        LOG_ERR("LINK_ZIG not ready");
        return;
    }
    gpio_pin_configure_dt(&link_zig_gpio, GPIO_OUTPUT);
    gpio_pin_set_dt(&link_zig_gpio, 0);

    if (!gpio_is_ready_dt(&mode_zig_gpio)) {
        LOG_ERR("MODE_ZIG not ready");
        return;
    }
    gpio_pin_configure_dt(&mode_zig_gpio, GPIO_OUTPUT);
    gpio_pin_set_dt(&mode_zig_gpio, 0);

    if (!device_is_ready(uart_zigbee_dev)) {
        LOG_ERR("UART_ZIGBEE not ready");
        return;
    }

    uart_irq_callback_user_data_set(uart_zigbee_dev, zigbee_uart_cb, NULL);
    uart_irq_rx_enable(uart_zigbee_dev);
    LOG_INF("Zigbee init done");
}

void zigbee_rx_thread_entry(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct tx_msg rx_msg;
    uint8_t reply[TX_MSG_MAX_DATA];
    size_t reply_len = 0;
    worker_t *worker = get_worker();
    worker->source = CMD_SOURCE_ZIGBEE;
    worker->tx_msg = get_zigbee_tx_msgq();

    while (1) {
        if (k_msgq_get(&zigbee_rx_msgq, &rx_msg, K_FOREVER) != 0) {
            continue;
        }

        LOG_INF("Zigbee RX %u bytes", rx_msg.len);
        LOG_HEXDUMP_INF(rx_msg.data, rx_msg.len, "Zigbee Cmd:");

        int ret = do_work(worker, rx_msg.data, rx_msg.len, reply, &reply_len);

        if (ret == 1  && reply_len > 0) {
            struct tx_msg tx_msg;
            memset(&tx_msg, 0, sizeof(tx_msg));
            memcpy(tx_msg.data, reply, reply_len);
            tx_msg.len = reply_len;
            if (k_msgq_put(worker->tx_msg, &tx_msg, K_NO_WAIT) != 0) {
                LOG_WRN("Zigbee TX queue full");
            } else {
                LOG_INF("Zigbee reply enqueued");
            }
        }
    }
    free_worker(worker);
}

void zigbee_tx_thread_entry(void *p1, void *p2, void *p3) {
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
