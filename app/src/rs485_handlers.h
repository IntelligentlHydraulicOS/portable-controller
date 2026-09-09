#ifndef RS485_HANDLERS_H
#define RS485_HANDLERS_H

#include <stdint.h>
#include <stddef.h>
#include <zephyr/kernel.h>

void rs485_send(const uint8_t *data, size_t len);

int rs485_init(void);
void rs485_rx_thread_entry(void *p1, void *p2, void *p3);
void rs485_tx_thread_entry(void *p1, void *p2, void *p3);
struct k_msgq *get_rs485_tx_msgq(void);
void rs485_test_thread(void *p1, void *p2, void *p3);
#endif