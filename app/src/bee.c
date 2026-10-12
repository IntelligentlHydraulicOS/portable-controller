#include "bee.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buzzer_ctrl, LOG_LEVEL_INF);

static const struct gpio_dt_spec buzzer = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), buzzer_gpios);

int buzzer_init(void)
{
    if (!device_is_ready(buzzer.port)) {
        LOG_ERR("GPIO NOT ready for buzzer");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&buzzer, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("failed to configure buzzer pin: %d", ret);
        return ret;
    }
    LOG_INF("Buzzer GPIO configured");
    return 0;
}

void buzzer_on(void)
{
    gpio_pin_set_dt(&buzzer, 1);
}


void buzzer_off(void)
{
    gpio_pin_set_dt(&buzzer, 0);
}

void test_buzzer(void)
{
    while (1) {
        LOG_INF("Buzzer: ON");
        buzzer_on();
        k_sleep(K_MSEC(500));

        LOG_INF("Buzzer: OFF");
        buzzer_off();
        k_sleep(K_MSEC(500));
    }
}