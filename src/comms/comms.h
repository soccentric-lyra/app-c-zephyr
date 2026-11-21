/**
 * @file    comms.h
 * @brief   Comms thread interface (HWLINK on USART3).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZFN-003 (COM-001..COM-007), ZFN-005
 */

#ifndef ZDEMO_COMMS_H
#define ZDEMO_COMMS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Report whether USART3 RX is running.
 *
 * @return true when running.
 */
bool comms_rx_running(void);

/**
 * @brief Wake the Comms thread when the USART3 RX ring has data.
 * @note ISR-safe; drops the notification when the queue is full.
 */
void comms_post_rx_from_isr(void);

/**
 * @brief Queue a button event to the Comms thread without blocking.
 * @details Comms updates its LED mirror and, after a Hello, sends a
 *          ButtonEvent.  Dropped when the queue is full.
 *
 * @param[in] press   1 = short, 2 = long.
 * @param[in] pattern New LD1/LD2 APP_PATTERN_*.
 * @param[in] period  New LD1/LD2 period, ms.
 */
void comms_post_button_event(uint8_t press, uint8_t pattern, uint16_t period);

#endif /* ZDEMO_COMMS_H */
