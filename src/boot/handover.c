/**
 * @file    handover.c
 * @brief   BOOT hand-over record in RTC_BKP2R and the stay-in-BOOT request.
 * @details VBAT is tied to VDD on the NUCLEO-F767ZI, so the backup registers
 *          survive a system reset but not a power cycle.  Layout is in
 *          boot_contract.h.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI
 * @srs     ZBT-004, ZBT-005
 */

#include <soc.h>
#include <zephyr/kernel.h>

#include "boot/boot_contract.h"
#include "boot/handover.h"

/** Decoded hand-over; written by app_handover_read() and Health. */
app_handover_t g_handover;

/**
 * @brief Clock PWR and set PWR_CR1.DBP to unlock the backup domain.
 * @note Zephyr has no generic backup-register API for the F7, so this
 *       touches PWR and RCC directly.
 */
static void backup_domain_enable(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	(void)RCC->APB1ENR;
	PWR->CR1 |= PWR_CR1_DBP;
	__DSB();
}

/**
 * @brief Decode RTC_BKP2R into @ref g_handover.
 * @details A missing magic or a slot other than A/B leaves slot UNKNOWN,
 *          present false and trial 0.
 */
void app_handover_read(void)
{
	backup_domain_enable();

	uint32_t raw = RTC->BKP2R;

	g_handover.raw = raw;
	g_handover.present = (raw & APP_HANDOVER_MAGIC_MASK) == APP_HANDOVER_MAGIC;
	if (!g_handover.present) {
		g_handover.slot = APP_SLOT_UNKNOWN;
		g_handover.trial = 0U;
		g_handover.trial_count = 0U;
		return;
	}

	g_handover.slot = (uint8_t)((raw >> APP_HANDOVER_SLOT_SHIFT) &
				    APP_HANDOVER_SLOT_MASK);
	if (g_handover.slot != APP_SLOT_A && g_handover.slot != APP_SLOT_B) {
		g_handover.slot = APP_SLOT_UNKNOWN;
		g_handover.present = false;
		g_handover.trial = 0U;
		g_handover.trial_count = 0U;
		return;
	}

	g_handover.trial = ((raw >> APP_HANDOVER_PENDING_BIT) & 1U) != 0U ? 2U : 1U;
	g_handover.trial_count = (uint8_t)((raw >> APP_HANDOVER_TRIAL_SHIFT) &
					   APP_HANDOVER_TRIAL_MASK);
}

/**
 * @brief Write "STAY" to RTC_BKP1R so BOOT stays in the bootloader on reset.
 */
void app_handover_request_bootloader(void)
{
	backup_domain_enable();
	RTC->BKP1R = APP_STAY_IN_BOOT_MAGIC;
	__DSB();
}
