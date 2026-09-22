#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include "battery_monitor.h"

LOG_MODULE_REGISTER(battery_monitor, LOG_LEVEL_INF);

static const struct adc_dt_spec adc_ch =
    ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static bool is_charging = false;

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
    if (!adc_is_ready_dt(&adc_ch)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    int rc = adc_channel_setup_dt(&adc_ch);   /* ← 关键：配置通道 */
    if (rc != 0) {
        LOG_ERR("ADC channel setup failed: %d", rc);
        return rc;
    }

    LOG_INF("Battery monitor initialized");
    return 0;
}

void battery_monitor_set_charge_state(bool charging)
{
    is_charging = charging;
    LOG_INF("Battery state: %s", charging ? "Charging" : "Discharging");
}

int battery_monitor_get_voltage(int *voltage_mv)
{
    if (!adc_is_ready_dt(&adc_ch)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    int16_t raw_value;
    struct adc_sequence sequence = {
        .buffer = &raw_value,
        .buffer_size = sizeof(raw_value),
    };

    /* 用设备树参数初始化 sequence（通道、分辨率等） */
    int ret = adc_sequence_init_dt(&adc_ch, &sequence);
    if (ret < 0) {
        LOG_ERR("adc_sequence_init_dt failed: %d", ret);
        return ret;
    }

    ret = adc_read_dt(&adc_ch, &sequence);
    if (ret < 0) {
        LOG_ERR("ADC read failed: %d", ret);
        return ret;
    }

    /* 将原始值转为 mV（用设备树中的增益和参考） */
    int32_t val_mv = raw_value;
    ret = adc_raw_to_millivolts_dt(&adc_ch, &val_mv);
    if (ret < 0) {
        LOG_ERR("adc_raw_to_millivolts_dt failed: %d", ret);
        return ret;
    }

    /* val_mv 就是 P1.07 引脚电压，再乘分压系数 */
    int32_t battery_voltage = val_mv * BATTERY_DIV_NUMERATOR / BATTERY_DIV_DENOMINATOR;
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