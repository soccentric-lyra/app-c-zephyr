/**
 * @file    handover.h
 * @brief   BOOT hand-over record in RTC_BKP2R.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZBT-004, ZBT-005
 */

#ifndef ZDEMO_HANDOVER_H
#define ZDEMO_HANDOVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Decoded BOOT hand-over state.
 */
typedef struct {
	bool present;        /**< true when magic and slot are valid. */
	uint8_t slot;        /**< APP_SLOT_*. */
	uint8_t trial;       /**< 0 unknown, 1 confirmed, 2 pending. */
	uint8_t trial_count; /**< BOOT trial counter, 0..3. */
	uint32_t raw;        /**< RTC_BKP2R as read. */
} app_handover_t;

/**
 * @brief Decoded hand-over, set by app_handover_read().
 * @note Health sets trial to 1 after confirming; Comms reads it.
 */
extern app_handover_t g_handover;

/**
 * @brief Decode RTC_BKP2R into @ref g_handover.
 * @details A missing or invalid record leaves slot UNKNOWN and trial 0.
 */
void app_handover_read(void);

/**
 * @brief Write "STAY" to RTC_BKP1R so BOOT stays in the bootloader on reset.
 */
void app_handover_request_bootloader(void);

#endif /* ZDEMO_HANDOVER_H */
