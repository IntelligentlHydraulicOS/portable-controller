#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <zephyr/kernel.h>

#define THREAD_POOL_SIZE         5
#define THREAD_POOL_STACK_SIZE   2048     /* 视 send_all_logs 需求调整 */
#define THREAD_POOL_QUEUE_DEPTH  16
#define THREAD_POOL_PRIO         5        /* 抢占优先级，按需调整 */

typedef void (*thread_pool_task_fn)(void *arg);

typedef struct {
    thread_pool_task_fn fn;
    void *arg;
} thread_pool_task_t;

/* 在系统启动时调用一次，创建 5 个 worker 线程 */
int thread_pool_init(void);

/* 提交任务。timeout=K_NO_WAIT 表示队列满立即失败 */
int thread_pool_submit(thread_pool_task_fn fn, void *arg, k_timeout_t timeout);

#endif /* THREAD_POOL_H */