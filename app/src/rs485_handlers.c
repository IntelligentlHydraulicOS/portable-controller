#include "rs485_handlers.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define UART485_NODE DT_NODELABEL(uart21)
#define DE_NODE DT_NODELABEL(de)

static const struct device *uart485_dev = DEVICE_DT_GET(UART485_NODE);
static const struct gpio_dt_spec de_gpio = GPIO_DT_SPEC_GET(DE_NODE, gpios);

void rs485_send(const uint8_t *data, size_t len) {
    if (!device_is_ready(uart485_dev) || !data || len == 0) {
        return;
    }
    gpio_pin_set_dt(&de_gpio, 1);
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart485_dev, data[i]);
    }
    uart_poll_out(uart485_dev, '\r');
    uart_poll_out(uart485_dev, '\n');
    k_busy_wait(100);
    gpio_pin_set_dt(&de_gpio, 0);
}