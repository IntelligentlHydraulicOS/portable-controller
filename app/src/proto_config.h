#ifndef PROTO_CONFIG_H_
#define PROTO_CONFIG_H_

#include <stdbool.h>

#define PROTO_CFG_PATH "/lfs/proto.cfg"

typedef enum {
    PROTO_NONE,
    PROTO_BLE,
    PROTO_ZIGBEE,
} protocol_t;

int proto_config_load(bool *ble_enabled, bool *zigbee_enabled);
int proto_config_save(bool ble_enabled, bool zigbee_enabled);

#endif