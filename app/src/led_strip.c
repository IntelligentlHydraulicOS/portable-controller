#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>
#include "led_strip.h"

LOG_MODULE_REGISTER(led_cycle, LOG_LEVEL_INF);

#define STRIP_NODE DT_ALIAS(led_strip)
static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);

/* 2个 WS2811 芯片，对应 2 个像素 */
#define PIXEL_COUNT 2
static struct led_rgb pixels[PIXEL_COUNT];

void set_all_pixels(uint8_t r, uint8_t g, uint8_t b)
{
    LOG_INF("set_all_pixels: r=%u g=%u b=%u", r, g, b);
    for (int i = 0; i < PIXEL_COUNT; i++) {
        pixels[i].g = r;
        pixels[i].r = g;
        pixels[i].b = b;
    }
    
    int ret = led_strip_update_rgb(strip, pixels, PIXEL_COUNT);
    if (ret < 0) {
        LOG_ERR("Failed to update LED strip (%d)", ret);
    }
}

int init_led_strip(void)
{
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device not ready");
        return -ENODEV;
    }
    return 0;
}

void strip_control_test_thread(void *args)
{
    LOG_INF("WS2811 LED cycle start: RED -> GREEN -> YELLOW");

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