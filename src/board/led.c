/**
 * @file    led.c
 * @brief   LED thread: LD1/LD2 patterns and the LD3 error display.
 * @details The LED thread owns the pattern state; other threads post to its
 *          queue.  Transitions are computed from absolute uptime, so jitter
 *          does not accumulate.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZFN-001 (LED-001..LED-007), ZFN-002 (BTN-002/003)
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/zdemo.h"
#include "app/task_report.h"
#include "app/zdemo_threads.h"
#include "board/led.h"
#include "board/led_pattern.h"
#include "comms/comms.h"
#include "system/health.h"

LOG_MODULE_DECLARE(app);

#define LED_QUEUE_LENGTH 8U   /**< LED message queue depth. */
#define LED_CHECKIN_MS   500U /**< Health check-in interval, ms. */

#define LED_ERROR_FAST_PERIOD 100U          /**< LD3 health-failed blink period, ms. */

/**
 * @brief LED message types.
 */
typedef enum {
	LED_MSG_SET = 0,          /**< Set one LED's pattern and period. */
	LED_MSG_SET_ALL,          /**< Set LD1 and LD2 together. */
	LED_MSG_BUTTON,           /**< B1 press (1 = short, 2 = long). */
	LED_MSG_CLEAR_ERROR,      /**< Clear all error flags. */
	LED_MSG_HEALTH_FAILED,    /**< Raise APP_ERROR_HEALTH. */
	LED_MSG_ERROR             /**< Raise the given error flags. */
} led_msg_type_t;

/**
 * @brief LED queue item; only the fields for @ref led_msg_type_t are set.
 */
typedef struct {
	uint8_t type;    /**< @ref led_msg_type_t. */
	uint8_t led;     /**< APP_LED_ID_* (SET). */
	uint8_t pattern; /**< APP_PATTERN_* (SET, SET_ALL). */
	uint8_t press;   /**< 1 = short, 2 = long (BUTTON). */
	uint16_t period; /**< Period, ms (SET, SET_ALL). */
	uint32_t flags;  /**< APP_ERROR_* mask (ERROR). */
	bool force;      /**< Allow LD3 (SET). */
} led_msg_t;


/** Producers -> LED thread queue. */
K_MSGQ_DEFINE(led_msgq, sizeof(led_msg_t), LED_QUEUE_LENGTH, 4);

/** LD1 (green), alias led0. */
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
/** LD2 (blue), alias led1. */
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
/** LD3 (red), alias led2. */
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);


/**
 * @brief Drive one LED; no-op if its GPIO is not ready.
 *
 * @param[in] led LED GPIO.
 * @param[in] on  true to switch on.
 */
static void led_write(const struct gpio_dt_spec *led, bool on)
{
	if (gpio_is_ready_dt(led)) {
		(void)gpio_pin_set_dt(led, on ? 1 : 0);
	}
}



/**
 * @brief Set one LED's pattern, keeping the CHASE pair consistent.
 * @details CHASE always applies to LD1 and LD2.  Leaving CHASE on one of
 *          them turns the other off.  LD3 is independent.
 *
 * @param[in,out] cfg     Pattern state of LD1..LD3.
 * @param[in]     led     APP_LED_ID_*.
 * @param[in]     pattern APP_PATTERN_*.
 * @param[in]     period  Period, ms.
 * @param[in]     now     Uptime, ms; restarts the pattern phase.
 */
static void apply_set(led_cfg_t cfg[3], uint8_t led, uint8_t pattern,
		      uint16_t period, uint32_t now)
{
	if (led == APP_LED_ID_3) {
		cfg[2].pattern = pattern;
		cfg[2].period = period;
		cfg[2].start = now;
		return;
	}

	if (pattern == APP_PATTERN_CHASE) {
		cfg[0].pattern = APP_PATTERN_CHASE;
		cfg[0].period = period;
		cfg[0].start = now;
		cfg[1].pattern = APP_PATTERN_CHASE;
		cfg[1].period = period;
		cfg[1].start = now;
		return;
	}

	/* Leaving CHASE breaks the pair. */
	if (cfg[0].pattern == APP_PATTERN_CHASE ||
	    cfg[1].pattern == APP_PATTERN_CHASE) {
		if (led == APP_LED_ID_1) {
			cfg[1].pattern = APP_PATTERN_OFF;
			cfg[1].period = period;
			cfg[1].start = now;
		} else {
			cfg[0].pattern = APP_PATTERN_OFF;
			cfg[0].period = period;
			cfg[0].start = now;
		}
	}

	uint32_t index = (led == APP_LED_ID_2) ? 1U : 0U;

	cfg[index].pattern = pattern;
	cfg[index].period = period;
	cfg[index].start = now;
}

/**
 * @brief Apply one LED message to the pattern state and error flags.
 * @details A button press also forwards the new pattern to Comms.  SET for
 *          LD3 without force is ignored.
 *
 * @param[in,out] cfg         Pattern state of LD1..LD3.
 * @param[in]     msg         Message.
 * @param[in]     now         Uptime, ms.
 * @param[in,out] error_flags LD3 APP_ERROR_* mask.
 */
static void handle_message(led_cfg_t cfg[3], const led_msg_t *msg,
			   uint32_t now, uint32_t *error_flags)
{
	switch (msg->type) {
	case LED_MSG_SET:
		if (msg->led == APP_LED_ID_3 && !msg->force) {
			return;
		}
		apply_set(cfg, msg->led, msg->pattern, msg->period, now);
		break;

	case LED_MSG_SET_ALL:
		apply_set(cfg, APP_LED_ID_1, msg->pattern, msg->period, now);
		cfg[1].pattern = msg->pattern;
		cfg[1].period = msg->period;
		cfg[1].start = now;
		break;

	case LED_MSG_BUTTON:
		if (msg->press == 2U) {   /* long: restore defaults */
			LOG_INF("button: long press, default LED patterns");
			cfg[0].pattern = APP_PATTERN_HEARTBEAT;
			cfg[0].period = APP_LED_DEFAULT_PERIOD;
			cfg[0].start = now;
			cfg[1].pattern = APP_PATTERN_OFF;
			cfg[1].period = APP_LED_DEFAULT_PERIOD;
			cfg[1].start = now;
			comms_post_button_event(2U, APP_PATTERN_HEARTBEAT,
						APP_LED_DEFAULT_PERIOD);
		} else {
			uint16_t period = cfg[0].period;

			if (period < APP_LED_MIN_PERIOD ||
			    period > APP_LED_MAX_PERIOD) {
				period = APP_LED_DEFAULT_PERIOD;
			}
			uint8_t pattern = (uint8_t)led_pattern_next(cfg[0].pattern);

			LOG_INF("button: short press, LED pattern %u", pattern);

			apply_set(cfg, APP_LED_ID_1, pattern, period, now);
			cfg[1].pattern = pattern;
			cfg[1].period = period;
			cfg[1].start = now;
			comms_post_button_event(1U, pattern, period);
		}
		break;

	case LED_MSG_CLEAR_ERROR:
		*error_flags = 0U;
		break;

	case LED_MSG_HEALTH_FAILED:
		*error_flags |= APP_ERROR_HEALTH;
		break;

	case LED_MSG_ERROR:
		*error_flags |= msg->flags;
		break;

	default:
		break;
	}
}

/**
 * @brief LED thread: drive LD1..LD3 and process LED messages.
 * @details LD3 shows a fast blink for a failed health check, solid for a
 *          fault or watchdog reset, else its pattern.  Sleeps on the queue
 *          until the next transition or check-in.  Panics if a GPIO cannot be
 *          configured.
 *
 * @param p1 Unused.
 * @param p2 Unused.
 * @param p3 Unused.
 */
static void led_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	(void)k_thread_name_set(k_current_get(), "LED");

	if (!gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&led2) ||
	    !gpio_is_ready_dt(&led3) ||
	    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE) != 0 ||
	    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE) != 0 ||
	    gpio_pin_configure_dt(&led3, GPIO_OUTPUT_INACTIVE) != 0) {
		k_panic();
	}

	uint32_t now = k_uptime_get_32();
	led_cfg_t cfg[3] = {
		{ APP_PATTERN_HEARTBEAT, APP_LED_DEFAULT_PERIOD, now },
		{ APP_PATTERN_OFF,       APP_LED_DEFAULT_PERIOD, now },
		{ APP_PATTERN_OFF,       APP_LED_DEFAULT_PERIOD, now }
	};
	uint32_t error_flags = 0U;
	uint32_t last_checkin = now;
	struct task_report report = {0};

	for (;;) {
		TASK_REPORT(report, 1, "LED",
			    "drives LD1-LD3 patterns, LD3 shows health and fault state");
		now = k_uptime_get_32();
		bool led1_on = false;
		bool led2_on = false;
		bool led3_on = false;
		uint32_t next1 = LED_PATTERN_NO_TRANSITION;
		uint32_t next2 = LED_PATTERN_NO_TRANSITION;
		uint32_t next3 = LED_PATTERN_NO_TRANSITION;

		led_pattern_evaluate(&cfg[0], false, now, &led1_on, &next1);
		led_pattern_evaluate(&cfg[1], true, now, &led2_on, &next2);

		if ((error_flags & APP_ERROR_HEALTH) != 0U) {
			uint32_t pos = now % LED_ERROR_FAST_PERIOD;

			led3_on = pos < (LED_ERROR_FAST_PERIOD / 2U);
			next3 = (led3_on ? (LED_ERROR_FAST_PERIOD / 2U)
					 : LED_ERROR_FAST_PERIOD) - pos;
		} else if ((error_flags &
			    (APP_ERROR_FAULT | APP_ERROR_WATCHDOG)) != 0U) {
			led3_on = true;
			next3 = LED_PATTERN_NO_TRANSITION;
		} else {
			led_pattern_evaluate(&cfg[2], false, now, &led3_on, &next3);
		}

		led_write(&led1, led1_on);
		led_write(&led2, led2_on);
		led_write(&led3, led3_on);

		if ((now - last_checkin) >= LED_CHECKIN_MS) {
			health_checkin(HEALTH_TASK_LED);
			last_checkin = now;
		}

		uint32_t since = now - last_checkin;
		uint32_t wait = (since >= LED_CHECKIN_MS)
				? 1U : (LED_CHECKIN_MS - since);

		if (next1 < wait) {
			wait = next1;
		}
		if (next2 < wait) {
			wait = next2;
		}
		if (next3 < wait) {
			wait = next3;
		}
		if (wait == 0U) {
			wait = 1U;
		}

		led_msg_t msg;

		if (k_msgq_get(&led_msgq, &msg, K_MSEC(wait)) == 0) {
			now = k_uptime_get_32();
			handle_message(cfg, &msg, now, &error_flags);
			while (k_msgq_get(&led_msgq, &msg, K_NO_WAIT) == 0) {
				handle_message(cfg, &msg, now, &error_flags);
			}
		}
	}
}

K_THREAD_DEFINE(zdemo_led_thread, ZDEMO_LED_STACK, led_thread, NULL, NULL,
		NULL, ZDEMO_PRIO_LED, 0, 0);

/**
 * @brief Queue one LED message without blocking.
 *
 * @param[in] msg Message, copied into the queue.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
static int led_post(const led_msg_t *msg)
{
	return k_msgq_put(&led_msgq, msg, K_NO_WAIT);
}

/**
 * @brief Queue a SET message.
 *
 * @param[in] led     APP_LED_ID_*.
 * @param[in] pattern APP_PATTERN_*.
 * @param[in] period  Period, ms.
 * @param[in] force   true to allow LD3.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_set(uint8_t led, uint8_t pattern, uint16_t period, bool force)
{
	led_msg_t msg = {
		.type = LED_MSG_SET,
		.led = led,
		.pattern = pattern,
		.period = period,
		.force = force
	};

	return led_post(&msg);
}

/**
 * @brief Queue a SET_ALL message for LD1 and LD2.
 *
 * @param[in] pattern APP_PATTERN_*.
 * @param[in] period  Period, ms.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_set_all(uint8_t pattern, uint16_t period)
{
	led_msg_t msg = {
		.type = LED_MSG_SET_ALL,
		.pattern = pattern,
		.period = period
	};

	return led_post(&msg);
}

/**
 * @brief Queue a BUTTON message.
 *
 * @param[in] press 1 = short, 2 = long.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_button(uint8_t press)
{
	led_msg_t msg = {
		.type = LED_MSG_BUTTON,
		.press = press
	};

	return led_post(&msg);
}

/**
 * @brief Queue a CLEAR_ERROR message.
 *
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_clear_error(void)
{
	led_msg_t msg = { .type = LED_MSG_CLEAR_ERROR };

	return led_post(&msg);
}

/**
 * @brief Queue a HEALTH_FAILED message (LD3 fast blink).
 *
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_health_failed(void)
{
	led_msg_t msg = { .type = LED_MSG_HEALTH_FAILED };

	return led_post(&msg);
}

/**
 * @brief Queue an ERROR message.
 *
 * @param[in] flags APP_ERROR_* mask.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_error(uint32_t flags)
{
	led_msg_t msg = {
		.type = LED_MSG_ERROR,
		.flags = flags
	};

	return led_post(&msg);
}
