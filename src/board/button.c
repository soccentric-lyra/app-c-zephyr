/**
 * @file    button.c
 * @brief   User button B1 (PC13): GPIO interrupt and 30 ms debounce.
 * @details B1 reads high when pressed.  The ISR only wakes the thread;
 *          debounce and short/long classification use absolute uptime.
 *          Presses go to the LED thread, which owns the pattern state.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZFN-002 (BTN-001..BTN-004)
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "app/zdemo.h"
#include "app/task_report.h"
#include "app/zdemo_threads.h"
#include "board/button.h"
#include "board/led.h"
#include "system/health.h"

LOG_MODULE_DECLARE(app);

#define BUTTON_DEBOUNCE_MS  30U   /**< Level must hold this long, ms. */
#define BUTTON_SHORT_MAX_MS 1000U /**< Release before this = short press, ms. */
#define BUTTON_LONG_MS      2000U /**< Held this long = long press, ms. */
#define BUTTON_CHECKIN_MS   500U  /**< Health check-in interval, ms. */
#define BUTTON_POLL_MS      10U   /**< Maximum wait on the wake semaphore, ms. */

/** B1 GPIO (devicetree alias sw0). */
static const struct gpio_dt_spec button =
	GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
/** GPIO callback for the B1 interrupt. */
static struct gpio_callback button_cb;
/** Given by button_isr() to wake the Button thread. */
static K_SEM_DEFINE(button_sem, 0, 1);

/**
 * @brief Read the raw B1 level.
 *
 * @return true when the pin is high (pressed).
 */
static bool button_raw(void)
{
	return gpio_pin_get_raw(button.port, button.pin) > 0;
}

/**
 * @brief B1 edge interrupt: wake the Button thread.
 * @note ISR context.
 *
 * @param[in] port Unused.
 * @param[in] cb   Unused.
 * @param[in] pins Unused.
 */
static void button_isr(const struct device *port, struct gpio_callback *cb,
		       gpio_port_pins_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	k_sem_give(&button_sem);
}

/**
 * @brief Button thread: debounce B1 and post short/long presses.
 * @details Wakes on the ISR or every 10 ms.  Short press: released within
 *          1 s, posted on release.  Long press: held 2 s, posted once while
 *          held.  A release between 1 s and 2 s posts nothing.  Panics if the
 *          GPIO cannot be configured.
 *
 * @param p1 Unused.
 * @param p2 Unused.
 * @param p3 Unused.
 */
static void button_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	(void)k_thread_name_set(k_current_get(), "Button");

	if (!gpio_is_ready_dt(&button) ||
	    gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_DOWN) != 0) {
		k_panic();
	}

	gpio_init_callback(&button_cb, button_isr, BIT(button.pin));
	if (gpio_add_callback_dt(&button, &button_cb) != 0 ||
	    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH) != 0) {
		k_panic();
	}

	uint32_t now = k_uptime_get_32();
	bool stable = button_raw();
	bool candidate = stable;
	uint32_t candidate_since = now;
	uint32_t press_start = now;
	bool pressed = stable;
	bool long_sent = false;
	uint32_t last_checkin = now;
	struct task_report report = {0};

	for (;;) {
		TASK_REPORT(report, 2, "Button",
			    "debounces B1, short press cycles the LED pattern, long press resets it");
		(void)k_sem_take(&button_sem, K_MSEC(BUTTON_POLL_MS));
		now = k_uptime_get_32();
		bool raw = button_raw();

		if (raw != candidate) {
			candidate = raw;
			candidate_since = now;
		} else if (candidate != stable &&
			   (now - candidate_since) >= BUTTON_DEBOUNCE_MS) {
			stable = candidate;
			if (stable) {
				pressed = true;
				long_sent = false;
				press_start = now;
			} else {
				uint32_t duration = now - press_start;

				if (pressed && !long_sent) {
					if (duration < BUTTON_SHORT_MAX_MS) {
						(void)led_post_button(1U);
					} else if (duration >= BUTTON_LONG_MS) {
						(void)led_post_button(2U);
					}
				}
				pressed = false;
			}
		}

		if (pressed && !long_sent &&
		    (now - press_start) >= BUTTON_LONG_MS) {
			long_sent = true;
			(void)led_post_button(2U);
		}

		if ((now - last_checkin) >= BUTTON_CHECKIN_MS) {
			health_checkin(HEALTH_TASK_BUTTON);
			last_checkin = now;
		}
	}
}

K_THREAD_DEFINE(zdemo_button_thread, ZDEMO_BUTTON_STACK, button_thread, NULL,
		NULL, NULL, ZDEMO_PRIO_BUTTON, 0, 0);
