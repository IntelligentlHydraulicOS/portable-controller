#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include "watchdog.h"

LOG_MODULE_REGISTER(wdt_sample, LOG_LEVEL_INF);

#define WDT_TIMEOUT_MS   3000  // 看门狗超时时间 3 秒
#define FEED_INTERVAL_MS 1000  // 喂狗间隔 1 秒

static const struct device *wdt_dev;
static int wdt_channel_id;

static void wdt_timeout_callback(const struct device *dev, int channel_id)
{
    LOG_WRN("Watchdog timeout on channel %d, system will reset!", channel_id);
}

int dog_init(void)
{
    int err;

    wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
    if (!device_is_ready(wdt_dev)) {
        LOG_ERR("Watchdog device not ready");
        return 0;
    }

    struct wdt_timeout_cfg wdt_cfg = {
        .flags = WDT_FLAG_RESET_SOC, // 超时后复位整个 SoC
        .window = {
            .min = 0,
            .max = WDT_TIMEOUT_MS,
        },
        .callback = wdt_timeout_callback, // 可选
    };

    wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_cfg);
    if (wdt_channel_id < 0) {
        LOG_ERR("wdt_install_timeout failed: %d", wdt_channel_id);
        return 0;
    }

    err = wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
    if (err < 0) {
        LOG_ERR("wdt_setup failed: %d", err);
        return 0;
    }
    LOG_INF("Watchdog started, timeout %d ms", WDT_TIMEOUT_MS);

    return 0;
}

const struct device *get_dog()
{
    return wdt_dev;
}

int get_dog_channel()
{
    return wdt_channel_id;
}