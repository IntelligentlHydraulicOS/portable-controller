#ifndef BATTERY_SCAN_H
#define BATTERY_SCAN_H

#include <stdbool.h>
#include <zephyr/kernel.h>
#include "battery_monitor.h"

int battery_scan_timer(bool use_ble,bool use_zigbee);

#endif /* BATTERY_SCAN_H */