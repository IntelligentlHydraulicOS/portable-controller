#ifndef BLE_HANDLERS_H_
#define BLE_HANDLERS_H_

#include "message.h"

int ble_init(void);

int ble_send(ble_tx_type_t type, const uint8_t *data, uint16_t len);

void ble_rx_thread_entry(void *p1, void *p2, void *p3);
void ble_tx_thread_entry(void *p1, void *p2, void *p3);

struct k_msgq *get_ble_tx_msgq(void);

#endif