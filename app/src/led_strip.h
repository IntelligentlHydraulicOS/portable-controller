#ifndef LED_STRIP_H_
#define LED_STRIP_H_

void set_all_pixels(uint8_t r, uint8_t g, uint8_t b);
int init_leds(void);
void led_test_thread(void *args);

#endif