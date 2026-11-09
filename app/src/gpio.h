#ifndef GPIO_H
#define GPIO_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define LED_ON  0
#define LED_OFF 1

void buzzer_on(void);
void buzzer_off(void);
void enout_on(void);
void enout_off(void);
void enlock_on(void);
void enlock_off(void);
void set_all_pixels(uint8_t r, uint8_t g, uint8_t b);
void link_led(int on);
void running_led(int on);
int init_gpio(void);
int read_gpio_adout(void);
void enable_zigbee(void);
void disable_zigbee(void);
int read_zigbee_link_status(void);

#endif