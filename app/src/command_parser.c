#include "command_parser.h"
#include <string.h>
#include <errno.h>
#include<stdio.h>

#define TX_MSG_MAX_DATA 256

int parse_command(const uint8_t *data, size_t len,
                  uint8_t *reply, size_t *reply_len,
                  uint8_t source) {
    if (!data || len == 0 || !reply || !reply_len) {
        *reply_len = 0;
        return -EINVAL;
    }

    uint8_t cmd = data[0];
    const char *response = NULL;
    char buf[32];

    switch (cmd) {
    case 0x01:
        response = "STATUS:OK";
        break;
    case 0x02:
        if (len >= 2) {
            uint8_t state = data[1];
            snprintf(buf, sizeof(buf), "LED:%s", state ? "ON" : "OFF");
            response = buf;
        } else {
            response = "LED:ERR";
        }
        break;
    case 0x03:
        if (source == CMD_SOURCE_BT) {
            return 1;
        } else {
            response = "ZIG:FORBIDDEN";
        }
        break;
    case 0x04:
        response = "ZIG:READY";
        break;
    default:
        response = "UNKNOWN_CMD";
        break;
    }

    if (response) {
        size_t resp_len = strlen(response);
        if (resp_len < TX_MSG_MAX_DATA) {
            memcpy(reply, response, resp_len);
            *reply_len = resp_len;
            return 0;
        }
    }
    *reply_len = 0;
    return -ENOMEM;
}