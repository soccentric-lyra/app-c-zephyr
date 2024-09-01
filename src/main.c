/**
 * @file    main.c
 * @brief   Zephyr demo application entry point.
 * @details The four application threads (Health, Comms, Button, LED) are
 *          statically defined; main() runs the reset-safe start-up sequence and
 *          returns.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (Zephyr board nucleo_f767zi), started by BOOT
 * @srs     SRS-ZEPHYR-F767: ZBT-001..005, ZGEN-003..005, ZRT-001..006,
 *          ZFN-001..005, ZSAFE-001..004
 */

#include <soc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app/zdemo.h"
#include "board/led.h"
#include "boot/boot_contract.h"
#include "boot/handover.h"
#include "comms/uart_hwlink.h"
#include "system/app_fault.h"
#include "system/health.h"
#include "system/iwdg.h"
#include "system/reset_cause.h"

LOG_MODULE_DECLARE(app);

BUILD_ASSERT(CONFIG_FLASH_LOAD_OFFSET == 0xC000,
	     "ZBT-001: the application must be linked at 0x0800C000");
BUILD_ASSERT(CONFIG_FLASH_LOAD_SIZE == 0xF4000,
	     "ZBT-001: the application slot is 976 KB");

/**
 * @brief Report heap usage for Status; always 0/0.
 * @details The application is heapless: flatcc uses a static arena and
 *          malloc/free are not linked.
 *
 * @param[out] free_bytes     Set to 0.
 * @param[out] min_free_bytes Set to 0.
 */
void app_heap_stats(uint32_t *free_bytes, uint32_t *min_free_bytes)
{
	*free_bytes = 0U;
	*min_free_bytes = 0U;
}

/**
 * @brief Run the reset-safe start-up sequence, then return.
 * @details Order: fault boot stamp, watchdog feed, reset cause, hand-over
 *          decode, VTOR check, watchdog install, LD3 latch, USART3 bring-up.
 *          Panics if VTOR is wrong or USART3 cannot start.
 *
 * @return 0.
 */
int main(void)
{
	/* The fault record is in no-init RAM; bump its boot id first. */
	app_fault_boot_start();

	/* ZBT-002/APP-004: feed the BOOT-started watchdog before long init. */
	app_wdt_feed();

	/* ZGEN-005: keep the reset cause for Status, then clear it. */
	app_reset_cause_capture();

	/* ZBT-004: decode the hand-over record before Health starts. */
	app_handover_read();

	/* ZDBG-001: start-up line on the debug UART. */
	LOG_INF("start v%s %s, slot %c %s, reset %s", APP_VERSION_STR,
		APP_GIT_COMMIT,
		g_handover.slot == APP_SLOT_A ? 'A' :
		g_handover.slot == APP_SLOT_B ? 'B' : '?',
		g_handover.trial == 2U ? "on trial" :
		g_handover.trial == 1U ? "confirmed" : "no hand-over",
		app_reset_cause_text);

	/* ZBT-001/ZBT-002: the vector table must be the image's own table. */
	if (SCB->VTOR != (0x08000000UL + CONFIG_FLASH_LOAD_OFFSET)) {
		k_panic();
	}

	(void)app_wdt_init();

	/* LED-005: latch LD3 for the last fault or watchdog reset. */
	if (app_fault_is_valid()) {
		(void)led_post_error(APP_ERROR_FAULT);
		LOG_WRN("last fault recorded before this reset");
	}
	if (app_reset_was_error()) {
		(void)led_post_error(APP_ERROR_WATCHDOG);
	}

	/* COM-001/002: USART3 must be running before the health check. */
	if (!hwlink_uart_init()) {
		k_panic();
	}

	return 0;
}
