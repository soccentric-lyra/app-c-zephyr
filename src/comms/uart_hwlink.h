/**
 * @file    uart_hwlink.h
 * @brief   Interrupt-driven USART3 transport for HWLINK.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (PD8 TX, PD9 RX)
 * @srs     ZCON-005, ZFN-003 (COM-001, COM-002)
 */

#ifndef ZDEMO_UART_HWLINK_H
#define ZDEMO_UART_HWLINK_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Configure USART3 at 115200 8N1, no flow control, and start RX.
 * @note No other Zephyr user (console, shell, log) is on USART3 (COM-007).
 *
 * @return true on success, false if the device is not ready or rejects the
 *         configuration.
 */
bool hwlink_uart_init(void);

/**
 * @brief Change the baud rate; on a change RX restarts with an empty ring.
 *
 * @param[in] baud APP_UART_DEFAULT_BAUD or APP_UART_FAST_BAUD.
 * @return true on success, false for another rate or a driver error.
 */
bool hwlink_uart_set_baud(uint32_t baud);

/**
 * @brief Return the current baud rate.
 *
 * @return Baud rate, bits/s.
 */
uint32_t hwlink_uart_baud(void);

/**
 * @brief Read received bytes from the RX ring without blocking.
 *
 * @param[out] dst     Buffer.
 * @param[in]  max_len Size of @p dst, bytes.
 * @return Bytes read; 0 when the ring is empty.
 */
uint32_t hwlink_uart_read(uint8_t *dst, uint32_t max_len);

/**
 * @brief Report whether RX is enabled and the device is ready.
 *
 * @return true when running.
 */
bool hwlink_uart_rx_running(void);

/**
 * @brief Send one encoded frame and wait until its last stop bit is out.
 * @note Comms thread only; blocks up to @p timeout_ms plus 10 ms.
 *
 * @param[in] data       Frame bytes.
 * @param[in] len        Frame length, bytes (<= HWLINK_MAX_ENCODED).
 * @param[in] timeout_ms Completion timeout, ms.
 * @return true when sent (also for an empty frame), false on timeout or an
 *         oversized frame.
 */
bool hwlink_uart_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

#endif /* ZDEMO_UART_HWLINK_H */
