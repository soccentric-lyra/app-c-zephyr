/**
 * @file    led.h
 * @brief   LED pattern and LD3 error requests to the LED thread.
 * @details Every function queues one message without blocking; the LED
 *          thread owns the pattern state and the pins.  Callable from any
 *          thread.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZFN-001 (LED-001..LED-007), ZFN-002 (BTN-002/003)
 */

#ifndef ZDEMO_LED_H
#define ZDEMO_LED_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Set the pattern and period of one LED.
 *
 * @param[in] led     APP_LED_ID_1..APP_LED_ID_3; LD3 needs @p force.
 * @param[in] pattern APP_PATTERN_*.
 * @param[in] period  Period, ms (APP_LED_MIN_PERIOD..APP_LED_MAX_PERIOD).
 * @param[in] force   true to allow LD3.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_set(uint8_t led, uint8_t pattern, uint16_t period, bool force);

/**
 * @brief Set the pattern and period of LD1 and LD2.
 *
 * @param[in] pattern APP_PATTERN_*.
 * @param[in] period  Period, ms.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_set_all(uint8_t pattern, uint16_t period);

/**
 * @brief Report a B1 press.
 * @details Short press cycles the LD1/LD2 pattern; long press restores the
 *          defaults.
 *
 * @param[in] press 1 = short, 2 = long.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_button(uint8_t press);

/**
 * @brief Clear all LD3 error flags.
 *
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_clear_error(void);

/**
 * @brief Raise APP_ERROR_HEALTH (LD3 fast blink).
 *
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_health_failed(void);

/**
 * @brief Raise LD3 error flags.
 *
 * @param[in] flags APP_ERROR_* mask.
 * @return 0 when queued, -ENOMSG when the queue is full.
 */
int led_post_error(uint32_t flags);

#endif /* ZDEMO_LED_H */
