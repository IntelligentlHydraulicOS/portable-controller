#ifndef TIMER_H
#define TIMER_H

#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/sys/atomic.h>

#define WORKER_TIMER_NUM  4
typedef struct work_timer_t{
    //TODO add enable counter to avoid multiple start/stop
    bool is_active;
    int period_msecond;
    struct k_timer timer;
    atomic_t      pending_cmds;
    uint16_t      serial_num;
} work_timer_t;

int init_work_timer(work_timer_t *work_timer, int period_msecond, void (*callback)(struct k_timer *timer));
int start_work_timer(work_timer_t *work_timer);
int stop_work_timer(work_timer_t *work_timer);

//call back funcs
void timer_heartbeat_expiry_cb(struct k_timer  *timer);
void timer_action_expiry_cb(struct k_timer *timer);
void timer_bee_expiry_cb(struct k_timer *timer);
void timer_test_thread(void *args);
#endif
