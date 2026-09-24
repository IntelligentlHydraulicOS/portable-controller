#ifndef LED_STRIP_H_
#define LED_STRIP_H_


/* 颜色定义 (R, G, B) */
#define COLOR_GREEN    0xFF, 0x00, 0x00 
#define COLOR_RED     0x00, 0xFF, 0x00
#define COLOR_YELLOW 0xFF, 0xFF, 0x00

void set_all_pixels(uint8_t r, uint8_t g, uint8_t b);
int init_led_strip(void);
void strip_control_test_thread(void *args);

#endif