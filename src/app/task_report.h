/**
 * @file    task_report.h
 * @brief   Once-a-second "task <n> <name>: <purpose>" debug line per thread.
 * @details Each application thread calls TASK_REPORT() every loop iteration;
 *          it prints at most one line a second per thread:
 *
 *              [  12034] C-ZEP I task 2 Button: debounces B1, ...
 *
 *          The including file must have LOG_MODULE_DECLARE(app).
 */
#ifndef TASK_REPORT_H
#define TASK_REPORT_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/**
 * @brief Per-thread TASK_REPORT() state; zero-initialise it.
 */
struct task_report {
	uint32_t last_ms; /**< Uptime of the last printed line, in ms. */
	bool primed;      /**< true once the first line has been printed. */
};

/**
 * @brief Log the task line if 1 s has passed since the last one.
 *
 * @param state   struct task_report lvalue owned by the calling thread.
 * @param number  Task number printed in the line.
 * @param name    Task name string.
 * @param purpose One-line purpose string.
 */
#define TASK_REPORT(state, number, name, purpose)                              \
	do {                                                                   \
		uint32_t t_now_ = k_uptime_get_32();                           \
		if (!(state).primed || (t_now_ - (state).last_ms) >= 1000U) {  \
			(state).primed = true;                                 \
			(state).last_ms = t_now_;                              \
			LOG_INF("task %u %s: %s", (unsigned)(number), (name),  \
				(purpose));                                    \
		}                                                              \
	} while (0)

#endif /* TASK_REPORT_H */
