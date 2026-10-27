#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include "battery_monitor.h"
#include "led_control.h"

LOG_MODULE_REGISTER(battery_monitor, LOG_LEVEL_INF);

static const struct adc_dt_spec adc_voltage_ch = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct adc_dt_spec adc_current_ch = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

static battery_state_t battery_state = BAT_STATE_DISCHARGING;

/* ========== 电流换算参数 ==========
 * INA199A2 固定增益 100 V/V
 * Vout = I * R_shunt * Gain + VREF
 * I(mA) = (Vout_mV - VREF_mV) * 1000 / (R_shunt_mΩ * Gain)
 */
#define INA199_GAIN          100
#define R_SHUNT_MILLIOHM     10      /* 10mΩ = 0.01Ω */
#define VREF_MV              0       /* 单向检测接GND为0 */

/* ========== 电压-SOC 查找表（升序，单位 mV） ========== */
static const int dp_table[][2] = {
    {3240, 0},   {3500, 5},   {3732, 10},  {3771, 15},
    {3844, 20},  {3889, 30},  {3921, 40},  {3973, 50},
    {4023, 60},  {4046, 70},  {4053, 80},  {4062, 85},
    {4074, 90},  {4099, 95},  {4193, 100}
};
#define DP_TABLE_SIZE (sizeof(dp_table) / sizeof(dp_table[0]))

static const int cp_table[][2] = {
    {3240, 0},   {3718, 5},   {3771, 10},  {3813, 15},
    {3883, 20},  {3913, 30},  {3945, 40},  {4006, 50},
    {4046, 60},  {4059, 70},  {4077, 80},  {4096, 85},
    {4141, 90},  {4182, 95},  {4189, 100}
};
#define CP_TABLE_SIZE (sizeof(cp_table) / sizeof(cp_table[0]))

static int voltage_to_soc(const int table[][2], size_t size, int voltage_mv)
{
    if (voltage_mv >= table[size-1][0])
        return table[size-1][1];
    if (voltage_mv <= table[0][0])
        return table[0][1];

    for (size_t i = 0; i < size - 1; i++) {
        int v_high = table[i+1][0];
        int v_low  = table[i][0];
        int soc_high = table[i+1][1];
        int soc_low  = table[i][1];

        if (voltage_mv <= v_high && voltage_mv >= v_low) {
            return soc_low + (voltage_mv - v_low) * (soc_high - soc_low) / (v_high - v_low);
        }
    }
    return table[size-1][1];
}

static int read_adc_mv(const struct adc_dt_spec *ch, int *voltage_mv)
{
    if (!adc_is_ready_dt(ch)) {
        LOG_ERR("ADC channel not ready");
        return -ENODEV;
    }

    int16_t raw_value;
    struct adc_sequence sequence = {
        .buffer = &raw_value,
        .buffer_size = sizeof(raw_value),
    };

    int ret = adc_sequence_init_dt(ch, &sequence);
    if (ret < 0) {
        LOG_ERR("adc_sequence_init_dt failed: %d", ret);
        return ret;
    }

    ret = adc_read_dt(ch, &sequence);
    if (ret < 0) {
        LOG_ERR("ADC read failed: %d", ret);
        return ret;
    }

    int32_t val_mv = raw_value;
    ret = adc_raw_to_millivolts_dt(ch, &val_mv);
    if (ret < 0) {
        LOG_ERR("adc_raw_to_millivolts_dt failed: %d", ret);
        return ret;
    }

    *voltage_mv = val_mv;
    return 0;
}

int battery_monitor_init(void)
{
    int rc;

    if (!adc_is_ready_dt(&adc_voltage_ch) || !adc_is_ready_dt(&adc_current_ch)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    rc = adc_channel_setup_dt(&adc_voltage_ch);
    if (rc != 0) {
        LOG_ERR("Voltage ADC channel setup failed: %d", rc);
        return rc;
    }

    rc = adc_channel_setup_dt(&adc_current_ch);
    if (rc != 0) {
        LOG_ERR("Current ADC channel setup failed: %d", rc);
        return rc;
    }

    LOG_INF("Voltage monitor (P1.11/AIN4) and current monitor (P1.06/AIN2) initialized");
    return 0;
}

void battery_monitor_set_charge_state(battery_state_t state)
{
    battery_state = state;
    LOG_INF("Battery state: %s", battery_state == BAT_STATE_CHARGING ? "Charging" : "Discharging");
    if (battery_state == BAT_STATE_CHARGING) {
        led_control_blink_alt(255, 0, 0,   0, 0, 255,   250);
    } else if (battery_state == BAT_STATE_FULL) {
        led_control_static(255, 255, 0);
    }
}

int battery_monitor_get_voltage(int *voltage_mv)
{
    return read_adc_mv(&adc_voltage_ch, voltage_mv);
}

int battery_monitor_get_current(int32_t *current_ma)
{
    int voltage_mv;
    int ret = read_adc_mv(&adc_current_ch, &voltage_mv);
    if (ret < 0) {
        return ret;
    }

    int64_t numerator = (int64_t)(voltage_mv - VREF_MV) * 1000;
    int64_t denominator = (int64_t)R_SHUNT_MILLIOHM * INA199_GAIN;

    if (denominator == 0) {
        return -EINVAL;
    }

    *current_ma = (int32_t)(numerator / denominator);
    return 0;
}

int battery_monitor_get_capacity(int *capacity_pct)
{
    int voltage_mv;
    int ret = battery_monitor_get_voltage(&voltage_mv);
    if (ret < 0) {
        return ret;
    }

    const int (*table)[2] = battery_state == BAT_STATE_CHARGING ? cp_table : dp_table;
    size_t size = battery_state == BAT_STATE_CHARGING ? CP_TABLE_SIZE : DP_TABLE_SIZE;

    *capacity_pct = voltage_to_soc(table, size, voltage_mv);
    return 0;
}

int battery_monitor_get_level(int *level)
{
    int capacity;
    int ret = battery_monitor_get_capacity(&capacity);
    if (ret < 0) {
        return ret;
    }

    *level = capacity / 20;
    if (*level > 5) *level = 5;
    return 0;
}