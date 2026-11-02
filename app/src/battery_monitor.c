#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include "battery_monitor.h"
#include "led_control.h"

LOG_MODULE_REGISTER(battery_monitor, LOG_LEVEL_INF);

#define ADC_ASYNC_TIMEOUT_MS  100
#define BATTERY_DIV_NUMERATOR   15  
#define BATTERY_DIV_DENOMINATOR 2 

static const struct adc_dt_spec adc_voltage_ch = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct adc_dt_spec adc_current_ch = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);
static const struct adc_dt_spec ad_out_ch = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2);

static battery_state_t battery_state = BAT_STATE_DISCHARGING;

/* 异步 ADC 的 poll 信号 */
static struct k_poll_signal adc_voltage_signal;
static struct k_poll_signal adc_current_signal;
static struct k_poll_signal ad_out_signal;


static __aligned(4) int16_t adc_voltage_raw;
static __aligned(4) int16_t adc_current_raw;
static __aligned(4) int16_t ad_out_raw;

/* ========== 电流换算参数 ==========
 * INA199A2 固定增益 100 V/V
 * Vout = I * R_shunt * Gain + VREF
 * I(mA) = (Vout_mV - VREF_mV) * 1000 / (R_shunt_mΩ * Gain)
 */
#define INA199_GAIN          100
#define R_SHUNT_MILLIOHM     100     /* 0.1Ω = 100mΩ，对应图中的 R40 */
#define VREF_MV              0       /* 单向检测，REF 接地为 0 */

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

static int read_adc_mv_async(const struct adc_dt_spec *ch, 
                             struct k_poll_signal *sig,
                             int16_t *raw_buffer,
                             int *voltage_mv)
{
    if (!adc_is_ready_dt(ch)) {
        LOG_ERR("ADC channel not ready");
        return -ENODEV;
    }

    struct adc_sequence sequence = {
        .buffer = raw_buffer,
        .buffer_size = sizeof(int16_t),
        .channels = BIT(ch->channel_id),
        .resolution = ch->resolution,
    };

    int ret = adc_sequence_init_dt(ch, &sequence);
    if (ret < 0) {
        LOG_ERR("adc_sequence_init_dt failed: %d", ret);
        return ret;
    }

    /* 重置 poll signal，准备接收新的完成事件 */
    k_poll_signal_reset(sig);

    /* 发起异步采样 */
    ret = adc_read_async_dt(ch, &sequence, sig);
    if (ret < 0) {
        LOG_ERR("adc_read_async_dt failed: %d", ret);
        return ret;
    }

    /* 等待转换完成（带超时） */
    struct k_poll_event events[] = {
        K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                                 K_POLL_MODE_NOTIFY_ONLY,
                                 sig),
    };

    ret = k_poll(events, 1, K_MSEC(ADC_ASYNC_TIMEOUT_MS));
    if (ret != 0) {
        LOG_ERR("ADC async poll timeout/error: %d", ret);
        return (ret == -EAGAIN) ? -ETIMEDOUT : ret;
    }

    /* 检查 poll signal 的结果码 */
    unsigned int signaled;
    int result;
    k_poll_signal_check(sig, &signaled, &result);
    if (result != 0) {
        LOG_ERR("ADC conversion error: %d", result);
        return result;
    }

    /* 原始值转 mV */
    int32_t val_mv = *raw_buffer;
    ret = adc_raw_to_millivolts_dt(ch, &val_mv);
    if (ret < 0) {
        LOG_ERR("adc_raw_to_millivolts_dt failed: %d", ret);
        return ret;
    }
    
    LOG_DBG("ADC ch%d raw: %d, voltage: %d mV", ch->channel_id, *raw_buffer, val_mv);
    *voltage_mv = val_mv;
    return 0;
}

int battery_monitor_init(void)
{
    int rc;

    k_poll_signal_init(&adc_voltage_signal);
    k_poll_signal_init(&adc_current_signal);
    k_poll_signal_init(&ad_out_signal);

    if (!adc_is_ready_dt(&adc_voltage_ch) || !adc_is_ready_dt(&adc_current_ch) || !adc_is_ready_dt(&ad_out_ch)) {
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

    rc = adc_channel_setup_dt(&ad_out_ch);
    if (rc != 0) {
        LOG_ERR("Output ADC channel setup failed: %d", rc);
        return rc;
    }
    LOG_INF("Voltage monitor adc channel initialized");
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
    int pin_mv;
    int ret = read_adc_mv_async(&adc_voltage_ch, &adc_voltage_signal, 
                                &adc_voltage_raw, &pin_mv);
    if (ret < 0) {
        return ret;
    }
    int32_t battery_voltage = pin_mv * BATTERY_DIV_NUMERATOR / BATTERY_DIV_DENOMINATOR;
    *voltage_mv = battery_voltage;
    
    LOG_DBG("Pin: %d mV -> Battery: %d mV", pin_mv, battery_voltage);
    return 0;
}

int battery_monitor_get_adout_voltage(int *voltage_mv)
{
    int pin_mv;
    int ret = read_adc_mv_async(&ad_out_ch, &ad_out_signal, 
                                &ad_out_raw, &pin_mv);
    if (ret < 0) {
        return ret;
    }
    int32_t battery_voltage = (pin_mv + 500) / 1000;
    *voltage_mv = battery_voltage;
    LOG_INF("Pin: %d mV -> Battery: %d mV", pin_mv, battery_voltage);

    return 0;
}

int battery_monitor_get_current(int32_t *current_ma)
{
    int voltage_mv;
    int ret = read_adc_mv_async(&adc_current_ch, &adc_current_signal, 
                                &adc_current_raw, &voltage_mv);
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

int battery_monitor_calc_capacity(int voltage_mv, int *capacity_pct)
{
    int cell_voltage_mv = voltage_mv / 3; 
    const int (*table)[2] = battery_state == BAT_STATE_CHARGING ? cp_table : dp_table;
    size_t size = battery_state == BAT_STATE_CHARGING ? CP_TABLE_SIZE : DP_TABLE_SIZE;

    *capacity_pct = voltage_to_soc(table, size, cell_voltage_mv);
    return 0;

    // const int (*table)[2] = battery_state == BAT_STATE_CHARGING ? cp_table : dp_table;
    // size_t size = battery_state == BAT_STATE_CHARGING ? CP_TABLE_SIZE : DP_TABLE_SIZE;

    // *capacity_pct = voltage_to_soc(table, size, voltage_mv);
    // return 0;
}

int battery_monitor_get_capacity(int *capacity_pct)
{
    int voltage_mv;
    int ret = battery_monitor_get_voltage(&voltage_mv);
    if (ret < 0) {
        return ret;
    }
    return battery_monitor_calc_capacity(voltage_mv, capacity_pct);
}

int battery_monitor_get_level(int *level)
{
    int capacity;
    int ret = battery_monitor_get_capacity(&capacity);
    if (ret < 0) {
        return ret;
    }

    *level = (capacity + 10) / 20;
    if (*level > 5){
     *level = 5;
    }else if (*level < 0) {
        *level = 0;
    }   
    return 0;
}