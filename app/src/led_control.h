#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <zephyr/kernel.h>

/* LED 工作模式 */
typedef enum {
    LED_MODE_OFF = 0,      /* 熄灭 */
    LED_MODE_STATIC,       /* 常亮 color1 */
    LED_MODE_BLINK,        /* color1 与 熄灭 交替 */
    LED_MODE_BLINK_ALT,    /* color1 与 color2 交替 */
} led_mode_t;

/* LED 控制消息 */
struct led_msg {
    led_mode_t mode;
    uint8_t c1_r, c1_g, c1_b;
    uint8_t c2_r, c2_g, c2_b;
    uint32_t half_period_ms;    /* 两次切换之间的时间，0 视为默认 500ms */
};

void led_control_test_thread(void *args);
void led_thread_entry(void *p1, void *p2, void *p3);
int led_control_init(void);
int led_control_send(const struct led_msg *msg);
int led_control_off(void);
int led_control_static(uint8_t r, uint8_t g, uint8_t b);
int led_control_blink(uint8_t r, uint8_t g, uint8_t b, uint32_t half_period_ms);
int led_control_blink_alt(uint8_t r1, uint8_t g1, uint8_t b1,
                          uint8_t r2, uint8_t g2, uint8_t b2,
                          uint32_t half_period_ms);

#endif 