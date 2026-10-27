#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "led_control.h"
#include "led_strip.h"

LOG_MODULE_REGISTER(led_ctl, LOG_LEVEL_INF);

#define LED_MSGQ_DEPTH         8
#define LED_DEFAULT_HALF_MS    500

K_MSGQ_DEFINE(led_msgq, sizeof(struct led_msg), LED_MSGQ_DEPTH, 4);

static struct led_msg current;
static bool phase_on = true;   /* 当前处于"主色相位"还是"次色/熄灭相位" */

static void apply_current(void)
{
    switch (current.mode) {
    case LED_MODE_OFF:
        set_all_pixels(0, 0, 0);
        break;

    case LED_MODE_STATIC:
        set_all_pixels(current.c1_r, current.c1_g,  current.c1_b);
        break;

    case LED_MODE_BLINK:
        if (phase_on) {
            set_all_pixels(current.c1_r, current.c1_g,  current.c1_b);
        } else {
            set_all_pixels(0, 0, 0);
        }
        break;

    case LED_MODE_BLINK_ALT:
        if (phase_on) {
            set_all_pixels(current.c1_r, current.c1_g,  current.c1_b);
        } else {
            set_all_pixels( current.c2_r, current.c2_g, current.c2_b);
        }
        break;

    default:
        set_all_pixels(0, 0, 0);
        break;
    }
    k_sleep(K_MSEC(1));
}

static inline bool is_blinking(led_mode_t m)
{
    return (m == LED_MODE_BLINK || m == LED_MODE_BLINK_ALT);
}

void led_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    current = (struct led_msg){
        .mode = LED_MODE_STATIC,
        .c1_r = 30,
        .c1_g = 0,
        .c1_b = 0,
        .half_period_ms = LED_DEFAULT_HALF_MS,
    };
    phase_on = true;
    apply_current();

    struct led_msg msg;

    while (1) {
        k_timeout_t timeout = K_FOREVER;
        if (is_blinking(current.mode)) {
            uint32_t half = current.half_period_ms ? current.half_period_ms
                                                   : LED_DEFAULT_HALF_MS;
            timeout = K_MSEC(half);
        }

        int ret = k_msgq_get(&led_msgq, &msg, timeout);
        if (ret == 0) {
            if (msg.half_period_ms == 0) {
                msg.half_period_ms = LED_DEFAULT_HALF_MS;
            }
            current = msg;
            phase_on = true;
            apply_current();
            LOG_INF("LED mode=%d period=%ums", current.mode, current.half_period_ms);
        } else {
            phase_on = !phase_on;
            apply_current();
        }
    }
}

int led_control_init(void)
{
    if (init_leds() != 0) {
        LOG_ERR("LED strip hardware init failed");
        return -ENODEV;
    }
    LOG_INF("LED control init done");
    return 0;
}

int led_control_send(const struct led_msg *msg)
{
    if (!msg) {
        return -EINVAL;
    }
    /* 用 K_NO_WAIT，避免在中断/定时器回调里阻塞 */
    int ret = k_msgq_put(&led_msgq, msg, K_NO_WAIT);
    if (ret != 0) {
        LOG_WRN("LED msgq full, drop msg");
    }
    return ret;
}

int led_control_off(void)
{
    struct led_msg m = { .mode = LED_MODE_OFF };
    return led_control_send(&m);
}

int led_control_static(uint8_t r, uint8_t g, uint8_t b)
{
    struct led_msg m = {
        .mode = LED_MODE_STATIC,
        .c1_r = r, .c1_g = g, .c1_b = b,
    };
    return led_control_send(&m);
}

int led_control_blink(uint8_t r, uint8_t g, uint8_t b, uint32_t half_period_ms)
{
    struct led_msg m = {
        .mode = LED_MODE_BLINK,
        .c1_r = r, .c1_g = g, .c1_b = b,
        .half_period_ms = half_period_ms,
    };
    return led_control_send(&m);
}

int led_control_blink_alt(uint8_t r1, uint8_t g1, uint8_t b1,
                          uint8_t r2, uint8_t g2, uint8_t b2,
                          uint32_t half_period_ms)
{
    struct led_msg m = {
        .mode = LED_MODE_BLINK_ALT,
        .c1_r = r1, .c1_g = g1, .c1_b = b1,
        .c2_r = r2, .c2_g = g2, .c2_b = b2,
        .half_period_ms = half_period_ms,
    };
    return led_control_send(&m);
}

void led_control_test_thread(void *args)
{
    struct led_msg m;
    while(1){
        k_sleep(K_SECONDS(2));
        m.mode = LED_MODE_BLINK;
        m.c1_r = 30; m.c1_g = 0; m.c1_b = 0;
        m.half_period_ms = 500;
        led_control_send(&m);

        k_sleep(K_SECONDS(4));
        m.mode = LED_MODE_BLINK_ALT;
        m.c1_r = 30; m.c1_g = 0; m.c1_b = 0;
        m.c2_r = 0; m.c2_g = 30; m.c2_b = 0;
        m.half_period_ms = 500;
        led_control_send(&m);

        k_sleep(K_SECONDS(4));
        m.mode = LED_MODE_OFF;
        led_control_send(&m);


        m.mode = LED_MODE_BLINK;
        m.c1_r = 0; m.c1_g = 30; m.c1_b = 0;
        m.half_period_ms = 500;
        led_control_send(&m);

        k_sleep(K_SECONDS(4));
        m.mode = LED_MODE_BLINK_ALT;
        m.c1_r = 0; m.c1_g = 30; m.c1_b = 0;
        m.c2_r = 0; m.c2_g = 0; m.c2_b = 30;
        m.half_period_ms = 500;
        led_control_send(&m);

        k_sleep(K_SECONDS(4));
        m.mode = LED_MODE_OFF;
        led_control_send(&m);

    }
}