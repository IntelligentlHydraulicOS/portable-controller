#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include "battery_monitor.h"

LOG_MODULE_REGISTER(battery_monitor, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *adc_dev;
static bool is_charging = false;

/* ADC 配置（与设备树一致） */
#define ADC_CHANNEL_ID 3
#define ADC_RESOLUTION 12
#define ADC_GAIN_DIV 6
#define ADC_REF_VOLTAGE_MV 900

/* ========== 硬件分压系数 ==========
 * 电池电压 = V(P1.07) * (4/3) * (27.87 / 7.87)
 *          = V(P1.07) * 11148 / 2361   (约为 4.722)
 */
#define BATTERY_DIV_NUMERATOR   11148  /* 4 * 2787 */
#define BATTERY_DIV_DENOMINATOR 2361   /* 3 * 787  */

/* ========== 电压-SOC 查找表（升序排列，单位 mV） ========== */
/* DP 表：放电后静置电压 -> SOC */
static const int dp_table[][2] = {
    {3240, 0},
    {3500, 5},
    {3732, 10},
    {3771, 15},
    {3844, 20},
    {3889, 30},
    {3921, 40},
    {3973, 50},
    {4023, 60},
    {4046, 70},
    {4053, 80},
    {4062, 85},
    {4074, 90},
    {4099, 95},
    {4193, 100}
};
#define DP_TABLE_SIZE (sizeof(dp_table) / sizeof(dp_table[0]))

/* CP 表：充电后静置电压 -> SOC */
static const int cp_table[][2] = {
    {3240, 0},
    {3718, 5},
    {3771, 10},
    {3813, 15},
    {3883, 20},
    {3913, 30},
    {3945, 40},
    {4006, 50},
    {4046, 60},
    {4059, 70},
    {4077, 80},
    {4096, 85},
    {4141, 90},
    {4182, 95},
    {4189, 100}
};
#define CP_TABLE_SIZE (sizeof(cp_table) / sizeof(cp_table[0]))

/**
 * @brief 根据电压查表计算 SOC（线性插值）
 */
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
            /* 线性插值 */
            return soc_low + (voltage_mv - v_low) * (soc_high - soc_low) / (v_high - v_low);
        }
    }
    return table[size-1][1];
}

int battery_monitor_init(void)
{
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    LOG_INF("Battery monitor initialized");
    return 0;
}

void battery_monitor_set_charge_state(bool charging)
{
    is_charging = charging;
    LOG_DBG("Battery state: %s", charging ? "Charging" : "Discharging");
}

int battery_monitor_get_voltage(int *voltage_mv)
{
    if (!adc_dev) {
        LOG_ERR("ADC device not initialized");
        return -EINVAL;
    }

    int16_t raw_value;
    struct adc_sequence sequence = {
        .channels = BIT(ADC_CHANNEL_ID),
        .buffer = &raw_value,
        .buffer_size = sizeof(raw_value),
        .resolution = ADC_RESOLUTION,
        .oversampling = 0,
    };

    int ret = adc_read(adc_dev, &sequence);
    if (ret < 0) {
        LOG_ERR("ADC read failed: %d", ret);
        return ret;
    }

    /* 1. 计算 P1.07 引脚电压 (mV) */
    int32_t pin_voltage = (int32_t)raw_value * ADC_REF_VOLTAGE_MV * ADC_GAIN_DIV / (1 << ADC_RESOLUTION);

    /* 2. 换算为实际电池端电压 (mV) */
    int32_t battery_voltage = pin_voltage * BATTERY_DIV_NUMERATOR / BATTERY_DIV_DENOMINATOR;

    *voltage_mv = battery_voltage;
    return 0;
}

int battery_monitor_get_capacity(int *capacity_pct)
{
    int voltage_mv;
    int ret = battery_monitor_get_voltage(&voltage_mv);
    if (ret < 0) {
        return ret;
    }

    const int (*table)[2] = is_charging ? cp_table : dp_table;
    size_t size = is_charging ? CP_TABLE_SIZE : DP_TABLE_SIZE;

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