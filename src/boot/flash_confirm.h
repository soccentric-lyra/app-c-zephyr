/**
 * @file    flash_confirm.h
 * @brief   Program the running slot's CONFIRMED metadata record.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZBT-006, ZFN-004 (HLT-002, HLT-005), ZSAFE-001/002
 */

#ifndef ZDEMO_FLASH_CONFIRM_H
#define ZDEMO_FLASH_CONFIRM_H

#include <stdbool.h>

/**
 * @brief Confirm the running slot by programming its CONFIRMED record.
 * @details Programs the record only when it is erased and never erases.
 *          Idempotent when the record is already valid.  Blocks on flash.
 *
 * @return true when a valid CONFIRMED record is present afterwards.
 * @retval false Flash error, or the record is neither valid nor erased.
 */
bool app_flash_confirm_running_slot(void);

#endif /* ZDEMO_FLASH_CONFIRM_H */
