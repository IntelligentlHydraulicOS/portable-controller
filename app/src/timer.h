#ifndef TIMER_H
#define TIMER_H

#include <zephyr/kernel.h>
#include <string.h>


void timer_heartbeat_expiry_cb(struct k_timer *timer_id);
void timer_action_expiry_cb(struct k_timer *timer_id);


#endif
