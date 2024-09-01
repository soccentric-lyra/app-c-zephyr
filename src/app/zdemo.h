/**
 * @file    zdemo.h
 * @brief   Shared constants for the Zephyr demo application.
 * @details Firmware identity, thread priorities and stacks, LED ids and
 *          patterns, UART baud rates, LD3 error bits and health-state codes.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (Zephyr board nucleo_f767zi)
 * @srs     SRS-ZEPHYR-F767
 */

#ifndef ZDEMO_H
#define ZDEMO_H

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>

/** Firmware name in HelloReply and Status (overridable). */
#ifndef APP_FIRMWARE_NAME
#define APP_FIRMWARE_NAME "zephyr-app"
#endif
/** Firmware version string (overridable). */
#ifndef APP_VERSION_STR
#define APP_VERSION_STR "0.1.0"
#endif
/** Git commit id, set at build time. */
#ifndef APP_GIT_COMMIT
#define APP_GIT_COMMIT "unknown"
#endif
/** Build date, set at build time. */
#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE "unknown"
#endif

/* ZRT-001: Health highest, Comms high, Button and LED normal. */
#define ZDEMO_PRIO_HEALTH  2 /**< Health thread priority (highest). */
#define ZDEMO_PRIO_COMMS   3 /**< Comms thread priority. */
#define ZDEMO_PRIO_BUTTON  4 /**< Button thread priority. */
#define ZDEMO_PRIO_LED     5 /**< LED thread priority (lowest). */

#define ZDEMO_HEALTH_STACK 2048 /**< Health thread stack, bytes. */
#define ZDEMO_COMMS_STACK  4096 /**< Comms thread stack, bytes. */
#define ZDEMO_BUTTON_STACK 1024 /**< Button thread stack, bytes. */
#define ZDEMO_LED_STACK    1024 /**< LED thread stack, bytes. */

/* LED identifiers and patterns ([DEMO] Appendix A, [LINK]). */
#define APP_LED_ID_1       1U /**< LD1, user LED. */
#define APP_LED_ID_2       2U /**< LD2, user LED. */
#define APP_LED_ID_3       3U /**< LD3, error LED (reserved). */

#define APP_PATTERN_OFF        0U /**< Off. */
#define APP_PATTERN_ON         1U /**< Steady on. */
#define APP_PATTERN_BLINK      2U /**< 50 % blink at the period. */
#define APP_PATTERN_HEARTBEAT  3U /**< Two 50 ms pulses per period. */
#define APP_PATTERN_FAST       4U /**< Blink at a fifth of the period. */
#define APP_PATTERN_CHASE      5U /**< LD1 and LD2 lit alternately. */
#define APP_PATTERN_MORSE_SOS  6U /**< Morse "SOS", 100 ms unit. */
#define APP_PATTERN_COUNT      7U /**< Number of patterns. */

#define APP_LED_MIN_PERIOD     50U   /**< Minimum LED period, ms. */
#define APP_LED_MAX_PERIOD     5000U /**< Maximum LED period, ms. */
#define APP_LED_DEFAULT_PERIOD 1000U /**< Default LED period, ms. */

#define APP_UART_DEFAULT_BAUD  115200U /**< Default HWLINK baud rate. */
#define APP_UART_FAST_BAUD     921600U /**< Fast HWLINK baud rate. */

/* Latched diagnostic state shown on LD3. */
#define APP_ERROR_FAULT      (1UL << 0U) /**< Fault from a previous boot. */
#define APP_ERROR_WATCHDOG   (1UL << 1U) /**< Last reset was the watchdog. */
#define APP_ERROR_HEALTH     (1UL << 2U) /**< Boot health check failed. */

/* Health state values used by Status. */
#define APP_HEALTH_NOT_REQUIRED 0U /**< Not a trial boot; no check. */
#define APP_HEALTH_RUNNING      1U /**< Trial boot, check pending. */
#define APP_HEALTH_PASSED       2U /**< Check passed, slot confirmed. */
#define APP_HEALTH_FAILED       3U /**< Check failed. */

/**
 * @brief Report heap usage for Status; always 0/0 (heapless).
 *
 * @param[out] free_bytes     Free heap, bytes.
 * @param[out] min_free_bytes Minimum free heap, bytes.
 */
void app_heap_stats(uint32_t *free_bytes, uint32_t *min_free_bytes);

#endif /* ZDEMO_H */
