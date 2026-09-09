#ifndef ZIGBEE_HANDLERS_H
#define ZIGBEE_HANDLERS_H

#include <zephyr/kernel.h>

int zigbee_init(void);
void zigbee_rx_thread_entry(void *p1, void *p2, void *p3);
void zigbee_tx_thread_entry(void *p1, void *p2, void *p3);

struct k_msgq *get_zigbee_tx_msgq(void);
#endif
