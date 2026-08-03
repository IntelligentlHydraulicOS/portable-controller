#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

#define TX_MSG_MAX_DATA 256
#define TX_MSG_QUEUE_SIZE 10

struct tx_msg {
    uint8_t data[TX_MSG_MAX_DATA];
    uint16_t len;
};


#endif