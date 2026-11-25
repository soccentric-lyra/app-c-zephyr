/**
 * @file    led_pattern.h
 * @brief   Pure LED pattern engine: segment expansion and phase evaluation.
 * @details Kept away from Zephyr and the GPIO layer so the same code can run
 *          in the firmware and in the native_sim ztest suite.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (and native_sim for tests)
 * @srs     LED-001..LED-007, BTN-002
 * @ingroup led_task
 */

#ifndef ZDEMO_LED_PATTERN_H
#define ZDEMO_LED_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Sentinel delay meaning "no further level change". */
#define LED_PATTERN_NO_TRANSITION 0xFFFFFFFFUL

/** @brief Maximum segments in one pattern cycle (Morse SOS uses 18). */
#define LED_PATTERN_MAX_SEGMENTS 20U

/** @brief Pattern state of one LED. */
typedef struct {
	uint8_t pattern; /**< APP_PATTERN_*. */
	uint16_t period; /**< Period, ms. */
	uint32_t start;  /**< Uptime of pattern phase 0, ms. */
} led_cfg_t;

/**
 * @brief Return the next pattern id, wrapping at APP_PATTERN_COUNT.
 * @param[in] current Current APP_PATTERN_*.
 * @return Next APP_PATTERN_*.
 */
uint32_t led_pattern_next(uint32_t current);

/**
 * @brief Expand a pattern into on/off segments covering one cycle.
 * @param[in]  cfg        Pattern state.
 * @param[in]  second_led true for LD2 of a CHASE pair.
 * @param[out] dur        Segment durations, ms.
 * @param[out] on         Segment levels.
 * @return Number of segments, 0..18.
 */
uint32_t led_pattern_segments(const led_cfg_t *cfg, bool second_led,
			      uint16_t dur[LED_PATTERN_MAX_SEGMENTS],
			      bool on[LED_PATTERN_MAX_SEGMENTS]);

/**
 * @brief Compute an LED's level and the time to its next change.
 * @param[in]  cfg        Pattern state.
 * @param[in]  second_led true for LD2 of a CHASE pair.
 * @param[in]  now        Uptime, ms.
 * @param[out] level      Output level.
 * @param[out] next_ms    Time to the next change, ms, or
 *                        LED_PATTERN_NO_TRANSITION.
 */
void led_pattern_evaluate(const led_cfg_t *cfg, bool second_led, uint32_t now,
			  bool *level, uint32_t *next_ms);

#endif /* ZDEMO_LED_PATTERN_H */
