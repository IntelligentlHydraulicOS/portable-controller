#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>

typedef enum {
    BAT_STATE_DISCHARGING,
    BAT_STATE_CHARGING,
    BAT_STATE_FULL,
} battery_state_t;

int battery_monitor_init(void);
void battery_monitor_set_charge_state(battery_state_t state);
int battery_monitor_get_voltage(int *voltage_mv);
int battery_monitor_get_capacity(int *capacity_pct);
int battery_monitor_get_level(int *level);
int battery_monitor_get_current(int32_t *current_ma);
int battery_monitor_get_adout_voltage(int *voltage_mv);
int battery_monitor_calc_capacity(int voltage_mv, int *capacity_pct);
int battery_monitor_get_capacity(int *capacity_pct);
int battery_monitor_get_level(int *level);

#endif