#ifndef RS485_HANDLERS_H
#define RS485_HANDLERS_H

#include <stdint.h>
#include <stddef.h>

void rs485_send(const uint8_t *data, size_t len);

#endif