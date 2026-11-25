/**
 * @file    led_pattern.c
 * @brief   Pure LED pattern engine.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (and native_sim for tests)
 * @srs     LED-001..LED-007, BTN-002
 * @ingroup led_task
 */

#include "app/zdemo.h"
#include "board/led_pattern.h"

uint32_t led_pattern_next(uint32_t current)
{
	return (current + 1U) % APP_PATTERN_COUNT;
}

uint32_t led_pattern_segments(const led_cfg_t *cfg, bool second_led,
			      uint16_t dur[LED_PATTERN_MAX_SEGMENTS],
			      bool on[LED_PATTERN_MAX_SEGMENTS])
{
	uint32_t n = 0U;
	uint32_t period = cfg->period;

	switch (cfg->pattern) {
	case APP_PATTERN_OFF:
		break;

	case APP_PATTERN_ON:
		dur[n] = 0xFFFFU;
		on[n] = true;
		n++;
		break;

	case APP_PATTERN_BLINK:
		dur[n] = (uint16_t)(period / 2U);
		on[n] = true;
		n++;
		dur[n] = (uint16_t)(period - (period / 2U));
		on[n] = false;
		n++;
		break;

	case APP_PATTERN_FAST: {
		uint32_t fast = period / 5U;

		if (fast < 2U) {
			fast = 2U;
		}
		dur[n] = (uint16_t)(fast / 2U);
		on[n] = true;
		n++;
		dur[n] = (uint16_t)(fast - (fast / 2U));
		on[n] = false;
		n++;
		break;
	}

	case APP_PATTERN_HEARTBEAT: {
		uint32_t cycle = period < 250U ? 250U : period;

		dur[n] = 50U;
		on[n] = true;
		n++;
		dur[n] = 150U;
		on[n] = false;
		n++;
		dur[n] = 50U;
		on[n] = true;
		n++;
		if (cycle > 250U) {
			dur[n] = (uint16_t)(cycle - 250U);
			on[n] = false;
			n++;
		}
		break;
	}

	case APP_PATTERN_CHASE: {
		uint32_t half = period / 2U;

		if (second_led) {
			dur[n] = (uint16_t)half;
			on[n] = false;
			n++;
			dur[n] = (uint16_t)(period - half);
			on[n] = true;
		} else {
			dur[n] = (uint16_t)half;
			on[n] = true;
			n++;
			dur[n] = (uint16_t)(period - half);
			on[n] = false;
		}
		n++;
		break;
	}

	case APP_PATTERN_MORSE_SOS:
	default:
		/* ... --- ... with a 100 ms unit. */
		dur[n] = 100U; on[n] = true;  n++;
		dur[n] = 100U; on[n] = false; n++;
		dur[n] = 100U; on[n] = true;  n++;
		dur[n] = 100U; on[n] = false; n++;
		dur[n] = 100U; on[n] = true;  n++;
		dur[n] = 300U; on[n] = false; n++;
		dur[n] = 300U; on[n] = true;  n++;
		dur[n] = 100U; on[n] = false; n++;
		dur[n] = 300U; on[n] = true;  n++;
		dur[n] = 100U; on[n] = false; n++;
		dur[n] = 300U; on[n] = true;  n++;
		dur[n] = 300U; on[n] = false; n++;
		dur[n] = 100U; on[n] = true;  n++;
		dur[n] = 100U; on[n] = false; n++;
		dur[n] = 100U; on[n] = true;  n++;
		dur[n] = 100U; on[n] = false; n++;
		dur[n] = 100U; on[n] = true;  n++;
		dur[n] = 700U; on[n] = false; n++;
		break;
	}

	return n;
}

void led_pattern_evaluate(const led_cfg_t *cfg, bool second_led, uint32_t now,
			  bool *level, uint32_t *next_ms)
{
	uint16_t dur[LED_PATTERN_MAX_SEGMENTS];
	bool on[LED_PATTERN_MAX_SEGMENTS];

	if (cfg->pattern == APP_PATTERN_OFF) {
		*level = false;
		*next_ms = LED_PATTERN_NO_TRANSITION;
		return;
	}
	if (cfg->pattern == APP_PATTERN_ON) {
		*level = true;
		*next_ms = LED_PATTERN_NO_TRANSITION;
		return;
	}

	uint32_t count = led_pattern_segments(cfg, second_led, dur, on);
	uint32_t cycle = 0U;

	for (uint32_t i = 0U; i < count; i++) {
		cycle += dur[i];
	}
	if (cycle == 0U) {
		*level = false;
		*next_ms = LED_PATTERN_NO_TRANSITION;
		return;
	}

	uint32_t pos = (now - cfg->start) % cycle;
	uint32_t acc = 0U;

	for (uint32_t i = 0U; i < count; i++) {
		if (dur[i] == 0U) {
			continue;
		}
		if (pos < (acc + dur[i])) {
			*level = on[i];
			*next_ms = (acc + dur[i]) - pos;
			return;
		}
		acc += dur[i];
	}

	*level = false;
	*next_ms = cycle - pos;
}
