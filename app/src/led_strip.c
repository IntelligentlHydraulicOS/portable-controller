#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include "gpio.h"
#include "led_strip.h"

LOG_MODULE_REGISTER(led_cycle, LOG_LEVEL_INF);

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