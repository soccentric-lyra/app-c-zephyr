/*
 * test_led_pattern.c - LED pattern engine tests on native_sim
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  native_sim
 *
 * The suite links the application's src/board/led_pattern.c, so the segment
 * tables and phase boundaries are the ones the firmware runs.
 */

#include <zephyr/ztest.h>

#include "app/zdemo.h"
#include "board/led_pattern.h"

static uint32_t total(const uint16_t dur[LED_PATTERN_MAX_SEGMENTS], uint32_t n)
{
	uint32_t sum = 0U;

	for (uint32_t i = 0U; i < n; i++) {
		sum += dur[i];
	}
	return sum;
}

ZTEST(led_pattern, test_next_wraps)
{
	zassert_equal(led_pattern_next(APP_PATTERN_OFF), APP_PATTERN_ON);
	zassert_equal(led_pattern_next(APP_PATTERN_CHASE), APP_PATTERN_MORSE_SOS);
	zassert_equal(led_pattern_next(APP_PATTERN_MORSE_SOS), APP_PATTERN_OFF);
	zassert_equal(led_pattern_next(APP_PATTERN_COUNT), APP_PATTERN_ON);
}

ZTEST(led_pattern, test_segment_tables)
{
	uint16_t dur[LED_PATTERN_MAX_SEGMENTS];
	bool on[LED_PATTERN_MAX_SEGMENTS];
	led_cfg_t cfg = {.pattern = APP_PATTERN_OFF, .period = 1000U, .start = 0U};
	uint32_t n;

	zassert_equal(led_pattern_segments(&cfg, false, dur, on), 0U, "OFF");

	cfg.pattern = APP_PATTERN_ON;
	n = led_pattern_segments(&cfg, false, dur, on);
	zassert_equal(n, 1U, "ON segments");
	zassert_equal(dur[0], 0xFFFFU, "ON duration");
	zassert_true(on[0], "ON level");

	cfg.pattern = APP_PATTERN_BLINK;
	n = led_pattern_segments(&cfg, false, dur, on);
	zassert_equal(n, 2U, "BLINK segments");
	zassert_equal(dur[0], 500U, "BLINK on");
	zassert_equal(dur[1], 500U, "BLINK off");
	zassert_equal(total(dur, n), 1000U, "BLINK cycle");

	cfg.pattern = APP_PATTERN_HEARTBEAT;
	n = led_pattern_segments(&cfg, false, dur, on);
	zassert_equal(n, 4U, "HEARTBEAT segments");
	zassert_equal(dur[0], 50U, "heartbeat on 1");
	zassert_equal(dur[1], 150U, "heartbeat gap");
	zassert_equal(dur[2], 50U, "heartbeat on 2");
	zassert_equal(dur[3], 750U, "heartbeat rest");

	cfg.pattern = APP_PATTERN_MORSE_SOS;
	n = led_pattern_segments(&cfg, false, dur, on);
	zassert_equal(n, 18U, "SOS segments");
	zassert_equal(total(dur, n), 3400U, "SOS cycle");
}

ZTEST(led_pattern, test_chase_pair)
{
	uint16_t dur[LED_PATTERN_MAX_SEGMENTS];
	bool on[LED_PATTERN_MAX_SEGMENTS];
	led_cfg_t cfg = {.pattern = APP_PATTERN_CHASE, .period = 1000U, .start = 0U};
	bool level;
	uint32_t next;
	uint32_t n;

	n = led_pattern_segments(&cfg, false, dur, on);
	zassert_equal(n, 2U, "chase segments");
	zassert_true(on[0], "LD1 first half");
	zassert_false(on[1], "LD1 second half");

	n = led_pattern_segments(&cfg, true, dur, on);
	zassert_equal(n, 2U, "chase pair segments");
	zassert_false(on[0], "LD2 first half");
	zassert_true(on[1], "LD2 second half");

	led_pattern_evaluate(&cfg, false, 0U, &level, &next);
	zassert_true(level, "LD1 on at 0");
	led_pattern_evaluate(&cfg, true, 0U, &level, &next);
	zassert_false(level, "LD2 off at 0");
	led_pattern_evaluate(&cfg, false, 500U, &level, &next);
	zassert_false(level, "LD1 off at 500");
	led_pattern_evaluate(&cfg, true, 500U, &level, &next);
	zassert_true(level, "LD2 on at 500");
}

ZTEST(led_pattern, test_phase_boundaries)
{
	led_cfg_t cfg = {.pattern = APP_PATTERN_BLINK, .period = 1000U, .start = 0U};
	bool level;
	uint32_t next;

	led_pattern_evaluate(&cfg, false, 0U, &level, &next);
	zassert_true(level, "on at 0");
	zassert_equal(next, 500U, "next edge at 500");

	led_pattern_evaluate(&cfg, false, 499U, &level, &next);
	zassert_true(level, "still on at 499");
	zassert_equal(next, 1U, "one tick to the edge");

	led_pattern_evaluate(&cfg, false, 500U, &level, &next);
	zassert_false(level, "off at 500");
	zassert_equal(next, 500U, "next edge at 1000");

	led_pattern_evaluate(&cfg, false, 1000U, &level, &next);
	zassert_true(level, "wrapped to on");

	cfg.pattern = APP_PATTERN_OFF;
	led_pattern_evaluate(&cfg, false, 0U, &level, &next);
	zassert_false(level, "OFF level");
	zassert_equal(next, LED_PATTERN_NO_TRANSITION, "OFF has no transition");

	cfg.pattern = APP_PATTERN_ON;
	led_pattern_evaluate(&cfg, false, 0U, &level, &next);
	zassert_true(level, "ON level");
	zassert_equal(next, LED_PATTERN_NO_TRANSITION, "ON has no transition");

	/* Unsigned delta: phase still correct across the 32-bit wrap. */
	cfg.pattern = APP_PATTERN_BLINK;
	cfg.start = 0xFFFFFF00UL;
	led_pattern_evaluate(&cfg, false, 0x00000010UL, &level, &next);
	zassert_true(level, "on after wrap");
	zassert_equal(next, 228U, "phase after wrap");
}

ZTEST_SUITE(led_pattern, NULL, NULL, NULL, NULL, NULL);
