#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(led_cycle, LOG_LEVEL_INF);

static const struct gpio_dt_spec r_en   = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), r_led_en_gpios);
static const struct gpio_dt_spec g_en   = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), g_led_en_gpios);
static const struct gpio_dt_spec b_en   = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), b_led_en_gpios);
static const struct gpio_dt_spec linkled_en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), linkled_led_en_gpios);
static const struct gpio_dt_spec runningled_en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), running_led_en_gpios);

void set_all_pixels(uint8_t r, uint8_t g, uint8_t b)
{
    gpio_pin_set_dt(&r_en, r > 0 ? 1 : 0);
    gpio_pin_set_dt(&g_en, g > 0 ? 1 : 0);
    gpio_pin_set_dt(&b_en, b > 0 ? 1 : 0);
}

void link_led(int on)
{
    gpio_pin_set_dt(&linkled_en, on ? 1 : 0);
}

void running_led(int on)
{
    gpio_pin_set_dt(&runningled_en, on ? 1 : 0);
}

int init_leds(void)
{
     if (!device_is_ready(r_en.port)) {
        LOG_ERR("GPIO NOT ready for red LED");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&r_en, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("failed to configure red LED pin: %d", ret);
        return ret;
    }

    if (!device_is_ready(g_en.port)) {
        LOG_ERR("GPIO NOT ready for green LED");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&g_en, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("failed to configure green LED pin: %d", ret);
        return ret;
    }

    if (!device_is_ready(b_en.port)) {
        LOG_ERR("GPIO NOT ready for blue LED");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&b_en, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("failed to configure blue LED pin: %d", ret);
        return ret;
    }
    if (!device_is_ready(linkled_en.port)) {
        LOG_ERR("GPIO NOT ready for link LED");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&linkled_en, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("failed to configure link LED pin: %d", ret);
        return ret; 
    }

    if (!device_is_ready(runningled_en.port)) {
        LOG_ERR("GPIO NOT ready for running LED");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&runningled_en, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("failed to configure running LED pin: %d", ret);
        return ret;
    }

    return 0;
}

void led_test_thread(void *args)
{
    ARG_UNUSED(args);
    while (1) {
        LOG_INF(">>> (255,0,0) 应该红");
        set_all_pixels(255, 0, 0);
        k_sleep(K_SECONDS(3));

        LOG_INF(">>> (0,255,0) 应该绿");
        set_all_pixels(0, 255, 0);
        k_sleep(K_SECONDS(3));

        LOG_INF(">>> (0,0,255) 应该蓝");
        set_all_pixels(0, 0, 255);
        k_sleep(K_SECONDS(6));
    }
}