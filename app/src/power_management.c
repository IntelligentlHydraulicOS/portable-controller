#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/atomic.h>
#include <cmsis_core.h>

LOG_MODULE_REGISTER(power_management, LOG_LEVEL_INF);

static atomic_t wake_exception = ATOMIC_INIT(-1);

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	static uint8_t cnt = 0;

	cnt++;
	ARG_UNUSED(substate_id);

	if (state != PM_STATE_RUNTIME_IDLE) {
		LOG_ERR("Unsupported PM state: %d", state);
		return;
	}

	atomic_set(&wake_exception, -1);
	k_cpu_idle();
	int exception = atomic_get(&wake_exception);

	if (exception >= 16) {
		// LOG_INF("Woke by IRQ %d (exception %d)", exception - 16, exception);
	} else if (exception >= 0) {
		// LOG_INF("Woke by CPU exception %d", exception);
	} else {
		// LOG_INF("Woke without a recorded interrupt");
	}

	if (cnt > 200) {
		// printk("System has been in runtime idle state for %d times\n", cnt);
		cnt = 0;
	}
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	static uint8_t cnt = 0;

	cnt++;
	ARG_UNUSED(state);
	ARG_UNUSED(substate_id);
	atomic_set(&wake_exception, (atomic_val_t)__get_IPSR());
	if (cnt > 200) {
		// printk("System exited runtime idle state %d times\n", cnt);
		cnt = 0;
	}
}
