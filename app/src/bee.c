#include "gpio.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buzzer_ctrl, LOG_LEVEL_INF);


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