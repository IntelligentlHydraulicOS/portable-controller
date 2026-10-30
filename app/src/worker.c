#include <string.h>
#include <errno.h>
#include<stdio.h>
#include <stdlib.h>
#include "timer.h"
#include "worker.h"
#include "action.h"
#include "message.h"
#include "log.h"
#include "thread_pool.h"
#include <zephyr/kernel.h>
#include "led_control.h"
#include "bee.h"

uint8_t choose_controller_handler(worker_t *worker, uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len);
uint8_t choose_action_handler(worker_t *worker, uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len);
uint8_t cancel_action_handler(worker_t *worker, uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len);
uint8_t get_log_handler(worker_t *worker, uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len);

void init_worker(worker_t *worker) {
    worker->local_id = 0;
    worker->action_id = 0;
    worker->controller_id = 0;
    worker->is_connected = false;
    worker->is_ready = false;
}

worker_t *get_worker(void) {
    worker_t *worker = (worker_t *)malloc(sizeof(worker_t));
    if (worker == NULL) {
        return NULL;
    }else{
        memset(worker, 0, sizeof(worker_t));
        init_worker(worker);
        init_work_timer(&worker->timer_heartbeat, 1000, timer_heartbeat_expiry_cb);
        init_work_timer(&worker->timer_action, 200, timer_action_expiry_cb);
        //TODO just for test, should be 200
        init_work_timer(&worker->timer_bee, 2000, timer_bee_expiry_cb);
    }

    return worker;    
}


bool is_worker_ready(worker_t *worker) {
    return worker != NULL && worker->is_connected && worker->is_ready;
}

bool is_worker_connected(worker_t *worker) {
    return worker != NULL && worker->is_connected;
}

void free_worker(worker_t *worker) {
    if (worker != NULL) {
        free(worker);
    }
}

command_handler_t g_command_handlers[] = {
    {0x00, choose_controller_handler},
    {0x01, choose_action_handler},
    {0xFF, cancel_action_handler},
    {0x02, get_log_handler},
};

uint8_t Xor(const void* data, int length){
    if (data == NULL) {
        return 0;
    }
    uint8_t xor = 0;
    uint8_t* p = (uint8_t *)data;
    while(length--){
        xor ^= *p++;
    }
    return xor;
}

uint8_t choose_controller_handler(worker_t *worker, uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len) {
    if (worker == NULL || data == NULL || len < 9 || reply == NULL || reply_len == NULL) {
        return 0;
    }

    if (data[3] == 0){
        worker->is_ready = false;
        stop_work_timer(&worker->timer_heartbeat);
        stop_work_timer(&worker->timer_action);
        stop_work_timer(&worker->timer_bee);
        worker->action_id = 0;
        uint8_t controller_id = data[len -2];
        worker->controller_id = controller_id;
        worker->is_connected = true;
        start_work_timer(&worker->timer_heartbeat);
        snprintf((char *)reply, TX_MSG_MAX_DATA, "connect to controller %u", worker->controller_id);
        *reply_len = strlen((char *)reply);
        return 1;
    }else{
        snprintf((char *)reply, TX_MSG_MAX_DATA, "choose not supported, please choose controller 0");
        *reply_len = strlen((char *)reply);
    }
    return 0;
}

uint8_t choose_action_handler(worker_t *worker, uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len) {
    if (worker == NULL || data == NULL || len < 9 || reply == NULL || reply_len == NULL) {
        return 0;
    }
    // uint8_t controller_id = data[len -2];
    // if (worker->controller_id != controller_id) {
    //     snprintf((char *)reply, TX_MSG_MAX_DATA, "Controller ID %u mismatch", controller_id);
    //     *reply_len = strlen((char *)reply);
    //     return 0;
    // }

    // if(is_worker_connected(worker) == false){
    //     snprintf((char *)reply, TX_MSG_MAX_DATA, "Worker not connected to controller");
    //     *reply_len = strlen((char *)reply);
    //     return 0;
    // }
    uint8_t action_id = data[3];
    worker->action_id = action_id;
    worker->is_ready = true;

    // action depends on the remote controller
    start_work_timer(&worker->timer_action);
    start_work_timer(&worker->timer_bee);
    snprintf((char *)reply, TX_MSG_MAX_DATA, "Action %u chosen", action_id);
    *reply_len = strlen((char *)reply);
    return 1;
}

uint8_t cancel_action_handler(worker_t *worker, uint8_t *data, size_t len, uint8_t *reply, size_t *reply_len) {
    if (worker == NULL || data == NULL || len < 9 || reply == NULL || reply_len == NULL) {
        return 0;
    }
        
    uint8_t controller_id = data[len -2];
    if (worker->controller_id != controller_id) {
        snprintf((char *)reply, TX_MSG_MAX_DATA, "Controller ID %u mismatch", controller_id);
        *reply_len = strlen((char *)reply);
        return 0;
    }

    if (is_worker_ready(worker)) {
        worker->action_id = 0;
        worker->is_ready = false;
        stop_work_timer(&worker->timer_action);
        stop_work_timer(&worker->timer_bee);
        snprintf((char *)reply, TX_MSG_MAX_DATA, "Action canceled");
        *reply_len = strlen((char *)reply); 
        return 1;
    }

    return 0;
}


static atomic_t log_sending = ATOMIC_INIT(0);
static void send_all_logs_task(void *arg)
{
    if (atomic_cas(&log_sending, 0, 1) == false) {
        return;
    }
    send_all_logs((worker_t *)arg);
    atomic_set(&log_sending, 0);
}

uint8_t get_log_handler(worker_t *worker, uint8_t *data, size_t len,
                        uint8_t *reply, size_t *reply_len)
{
    if (worker == NULL || data == NULL || len < 9 ||
        reply == NULL || reply_len == NULL) {
        return 0;
    }

    if (thread_pool_submit(send_all_logs_task, worker, K_NO_WAIT) != 0) {
        snprintf((char *)reply, TX_MSG_MAX_DATA, "Log queue full, try later");
        *reply_len = strlen((char *)reply);
        return 0;
    }

    snprintf((char *)reply, TX_MSG_MAX_DATA, "Logs queued");
    *reply_len = strlen((char *)reply);
    return 1;
}


uint8_t do_work(worker_t *worker, uint8_t *data, size_t len,
                  uint8_t *reply, size_t *reply_len) {
    if (!data || len != 9 || !reply || !reply_len) {
        if (reply_len) {
            *reply_len = 0;
        }
        return -EINVAL;
    }

    *reply_len = 0;
    uint8_t response = 0;

    if (data[0] != 0xAA || data[1]!= 0x55) {
        return 0;
    }
    if (Xor(data, len - 1) != data[len - 1]) {
        return 0;
    }

    uint8_t cmd = data[2]; 
    for (int i = 0; g_command_handlers[i].handler != NULL; i++) {
        if (g_command_handlers[i].id == cmd) {
            response = g_command_handlers[i].handler(worker, data, len, reply, reply_len);
            break;
        }
    }
    return response;
}