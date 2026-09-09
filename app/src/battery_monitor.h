#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>

int battery_monitor_init(void);
void battery_monitor_set_charge_state(bool charging);
int battery_monitor_get_voltage(int *voltage_mv);
int battery_monitor_get_capacity(int *capacity_pct);
int battery_monitor_get_level(int *level);
#endif