#ifndef BLE_HANDLERS_H
#define BLE_HANDLERS_H

#include <zephyr/kernel.h>

void ble_init(void);
void ble_rx_thread_entry(void *p1, void *p2, void *p3);
void ble_tx_thread_entry(void *p1, void *p2, void *p3);

struct k_msgq *get_ble_tx_msgq(void);

#endif