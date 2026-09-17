#include "thread_pool.h"
#include <zephyr/kernel.h>

K_THREAD_STACK_ARRAY_DEFINE(pool_stacks, THREAD_POOL_SIZE, THREAD_POOL_STACK_SIZE);
static struct k_thread pool_threads[THREAD_POOL_SIZE];

K_MSGQ_DEFINE(pool_msgq,
              sizeof(thread_pool_task_t),
              THREAD_POOL_QUEUE_DEPTH,
              4 /* 对齐 */);

static void pool_worker_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    thread_pool_task_t task;
    while (1) {
        if (k_msgq_get(&pool_msgq, &task, K_FOREVER) == 0) {
            if (task.fn != NULL) {
                task.fn(task.arg);
            }
        }
    }
}

int thread_pool_init(void)
{
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        k_tid_t tid = k_thread_create(&pool_threads[i],
                                      pool_stacks[i],
                                      K_THREAD_STACK_SIZEOF(pool_stacks[i]),
                                      pool_worker_entry,
                                      NULL, NULL, NULL,
                                      K_PRIO_PREEMPT(THREAD_POOL_PRIO),
                                      0, K_NO_WAIT);
        if (tid == NULL) {
            return -ENOMEM;
        }
        k_thread_name_set(tid, "pool_worker");
    }
    return 0;
}

int thread_pool_submit(thread_pool_task_fn fn, void *arg, k_timeout_t timeout)
{
    if (fn == NULL) {
        return -EINVAL;
    }
    thread_pool_task_t task = {
        .fn  = fn,
        .arg = arg,
    };
    return k_msgq_put(&pool_msgq, &task, timeout);
}