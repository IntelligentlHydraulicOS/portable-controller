#ifndef BATTERY_SCAN_H
#define BATTERY_SCAN_H

#include <stdbool.h>
#include <zephyr/kernel.h>
#include "battery_monitor.h"

typedef struct {
    int voltage; // in millivolts
    int capacity; // in percentage (0-100)
    int level; // number of bars (0-5)
    int current; // in milliamperes
    int adout_voltage; // in volts
} battery_info_t;

extern battery_info_t g_battery_info;

int battery_scan_timer(bool use_ble,bool use_zigbee);

#endif /* BATTERY_SCAN_H */