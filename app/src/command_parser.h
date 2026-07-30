#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>
#include <stddef.h>

#define CMD_SOURCE_BT     0
#define CMD_SOURCE_ZIGBEE 1
#define CMD_SOURCE_RS485  2


int parse_command(const uint8_t *data, size_t len,
                  uint8_t *reply, size_t *reply_len,
                  uint8_t source);

#endif