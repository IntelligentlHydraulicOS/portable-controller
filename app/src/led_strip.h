#ifndef LED_STRIP_H_
#define LED_STRIP_H_

#define LED_ON  1
#define LED_OFF 0

void set_all_pixels(uint8_t r, uint8_t g, uint8_t b);
int init_leds(void);
void led_test_thread(void *args);
void link_led(int on);
void running_led(int on);

#endif