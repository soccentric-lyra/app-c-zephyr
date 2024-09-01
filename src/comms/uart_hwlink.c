/**
 * @file    uart_hwlink.c
 * @brief   Interrupt-driven USART3 transport for HWLINK.
 * @details The ISR fills a 4 KB RX ring and drains the TX buffer; only the
 *          Comms thread waits for TX completion.  USART3 carries HWLINK only
 *          (COM-007).
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  NUCLEO-F767ZI (PD8 TX, PD9 RX)
 * @srs     ZCON-005, ZFN-003 (COM-001, COM-002)
 */

#include <soc.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include "app/zdemo.h"
#include "comms/hwlink.h"
#include "comms/uart_hwlink.h"

/** RX ring size, bytes. */
#define UART_RX_RING_SIZE 4096U
/** TX timeout, ms (unused; callers pass their own). */
#define UART_TX_TIMEOUT   200U

/** USART3 device. */
static const struct device *const uart = DEVICE_DT_GET(DT_NODELABEL(usart3));

/** RX ring; ISR producer, Comms consumer. */
RING_BUF_DECLARE(uart_rx_ring, UART_RX_RING_SIZE);

/** Given by the ISR when the last TX byte is loaded. */
static K_SEM_DEFINE(uart_tx_sem, 0, 1);

/** Frame being sent; drained by the ISR. */
static uint8_t uart_tx_buf[HWLINK_MAX_ENCODED];
/** Bytes in uart_tx_buf. */
static volatile uint16_t uart_tx_len;
/** Next uart_tx_buf byte to send. */
static volatile uint16_t uart_tx_pos;
/** true while RX interrupts feed the ring. */
static volatile bool uart_rx_enabled;
/** Current baud rate, bits/s. */
static uint32_t uart_baud = APP_UART_DEFAULT_BAUD;

/**
 * @brief Clear ORE/FE/NE/PE, discarding RDR, if any is set.
 * @note ISR context.
 */
static void uart_clear_errors(void)
{
	uint32_t isr = USART3->ISR;

	if ((isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE |
		    USART_ISR_PE)) != 0U) {
		volatile uint32_t dr = USART3->RDR;

		(void)dr;
		USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF |
			      USART_ICR_NCF | USART_ICR_PECF;
	}
}

/**
 * @brief USART3 interrupt: fill the RX ring, feed the TX FIFO.
 * @details Gives uart_tx_sem and disables TX interrupts once the last byte
 *          is loaded.  A full ring drops bytes.
 * @note ISR context.
 *
 * @param[in] dev       USART3 device.
 * @param[in] user_data Unused.
 */
static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	(void)uart_irq_update(dev);

	uart_clear_errors();

	while (uart_irq_rx_ready(dev)) {
		uint8_t chunk[32];
		int got = uart_fifo_read(dev, chunk, sizeof(chunk));

		if (got <= 0) {
			break;
		}
		(void)ring_buf_put(&uart_rx_ring, chunk, (uint32_t)got);
	}

	if (uart_irq_tx_ready(dev)) {
		int sent = uart_fifo_fill(dev, &uart_tx_buf[uart_tx_pos],
					  (int)(uart_tx_len - uart_tx_pos));

		if (sent > 0) {
			uart_tx_pos += (uint16_t)sent;
		}
		if (uart_tx_pos >= uart_tx_len) {
			uart_irq_tx_disable(dev);
			k_sem_give(&uart_tx_sem);
		}
	}
}

/**
 * @brief Flush the RX ring and enable RX and error interrupts.
 */
static void uart_rx_start(void)
{
	ring_buf_reset(&uart_rx_ring);
	uart_irq_rx_enable(uart);
	uart_irq_err_enable(uart);
	uart_rx_enabled = true;
}

/**
 * @brief Configure USART3 at 115200 8N1, no flow control, and start RX.
 *
 * @return true on success, false if the device is not ready or rejects the
 *         configuration.
 */
bool hwlink_uart_init(void)
{
	if (!device_is_ready(uart)) {
		return false;
	}

	const struct uart_config cfg = {
		.baudrate = APP_UART_DEFAULT_BAUD,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	if (uart_configure(uart, &cfg) != 0) {
		return false;
	}

	uart_irq_callback_set(uart, uart_isr);
	uart_rx_start();
	uart_baud = APP_UART_DEFAULT_BAUD;
	return true;
}

/**
 * @brief Change the baud rate with RX stopped.
 * @details On a driver error the previous rate is restored.  RX restarts
 *          with an empty ring either way.
 *
 * @param[in] baud APP_UART_DEFAULT_BAUD or APP_UART_FAST_BAUD.
 * @return true on success or no change, false for another rate or a driver
 *         error.
 */
bool hwlink_uart_set_baud(uint32_t baud)
{
	if (baud != APP_UART_DEFAULT_BAUD && baud != APP_UART_FAST_BAUD) {
		return false;
	}
	if (baud == uart_baud) {
		return true;
	}

	struct uart_config cfg;
	uint32_t old_baud = uart_baud;

	if (uart_config_get(uart, &cfg) != 0) {
		return false;
	}

	uart_irq_err_disable(uart);
	uart_irq_rx_disable(uart);
	uart_rx_enabled = false;

	cfg.baudrate = baud;
	if (uart_configure(uart, &cfg) != 0) {
		cfg.baudrate = old_baud;
		(void)uart_configure(uart, &cfg);
		uart_rx_start();
		return false;
	}

	uart_baud = baud;
	uart_rx_start();
	return true;
}

/**
 * @brief Return the current baud rate.
 *
 * @return Baud rate, bits/s.
 */
uint32_t hwlink_uart_baud(void)
{
	return uart_baud;
}

/**
 * @brief Read received bytes from the RX ring without blocking.
 *
 * @param[out] dst     Buffer.
 * @param[in]  max_len Size of @p dst, bytes.
 * @return Bytes read; 0 when empty.
 */
uint32_t hwlink_uart_read(uint8_t *dst, uint32_t max_len)
{
	return ring_buf_get(&uart_rx_ring, dst, max_len);
}

/**
 * @brief Report whether RX is enabled and the device is ready.
 *
 * @return true when running.
 */
bool hwlink_uart_rx_running(void)
{
	return uart_rx_enabled && device_is_ready(uart);
}

/**
 * @brief Send one frame and wait until its last stop bit is out (DBT-005).
 * @details Waits on uart_tx_sem, then polls ISR.TC for up to 10 ms so a baud
 *          change can follow immediately.
 *
 * @param[in] data       Frame bytes.
 * @param[in] len        Frame length, bytes (<= HWLINK_MAX_ENCODED).
 * @param[in] timeout_ms Completion timeout, ms.
 * @return true when sent (also for an empty frame), false on timeout or an
 *         oversized frame.
 */
bool hwlink_uart_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
	if (len == 0U || data == NULL) {
		return true;
	}
	if (len > sizeof(uart_tx_buf)) {
		return false;
	}

	/* Drop a stale completion, then hand the frame to the ISR. */
	(void)k_sem_take(&uart_tx_sem, K_NO_WAIT);
	memcpy(uart_tx_buf, data, len);
	uart_tx_len = len;
	uart_tx_pos = 0U;

	/* The STM32 driver raises TX interrupts on TC only, and TC is clear
	 * after BOOT and after every frame (TCCF below), so enabling the IRQ
	 * alone never starts a frame.  Load the first byte here, before the
	 * IRQ is enabled; its completion sets TC and the ISR sends the rest. */
	int primed = uart_fifo_fill(uart, uart_tx_buf, (int)len);

	if (primed > 0) {
		uart_tx_pos = (uint16_t)primed;
	}
	uart_irq_tx_enable(uart);

	if (k_sem_take(&uart_tx_sem, K_MSEC(timeout_ms)) != 0) {
		uart_irq_tx_disable(uart);
		return false;
	}

	/* DBT-005/APP-006: wait until the last stop bit has left the shift
	 * register, not merely until the DMA/FIFO has accepted the byte. */
	uint32_t start = k_uptime_get_32();

	while ((USART3->ISR & USART_ISR_TC) == 0U) {
		if ((k_uptime_get_32() - start) > 10U) {
			break;
		}
	}
	USART3->ICR = USART_ICR_TCCF;

	return true;
}
