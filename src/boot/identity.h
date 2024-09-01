/**
 * @file    identity.h
 * @brief   Factory PRV1 identity record reader (A-03).
 * @details Getters load the record on first use.  Not thread-safe: the
 *          cache is unlocked.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-04
 * @target  STM32F767ZIT6
 * @srs     identity contract PRV1, factory A-02/A-03/A-05/A-06
 */

#ifndef IDENTITY_H
#define IDENTITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Reload PRV1: physical bank-2 address first, then the swapped alias.
 *
 * @return true when a valid, UID-bound record was found.
 */
bool app_identity_refresh(void);

/**
 * @brief Report whether a valid, UID-bound record is present.
 *
 * @return true when valid.
 */
bool app_identity_valid(void);

/**
 * @brief Return the serial number.
 *
 * @return NUL-terminated serial; "" when absent.
 */
const char *app_identity_serial(void);

/**
 * @brief Return the product name.
 *
 * @return NUL-terminated name; "" when absent.
 */
const char *app_identity_product(void);

/**
 * @brief Return the hardware revision.
 *
 * @return NUL-terminated revision; "" when absent.
 */
const char *app_identity_hw_revision(void);

/**
 * @brief Return the provisioning profile.
 *
 * @return NUL-terminated profile; "" when absent.
 */
const char *app_identity_profile(void);

/**
 * @brief Return the record revision.
 *
 * @return Revision; 0 when absent.
 */
uint16_t app_identity_revision(void);

/**
 * @brief Copy the EUI-48 MAC.
 *
 * @param[out] mac 6-byte buffer.
 * @return true on success, false when no MAC is recorded.
 */
bool app_identity_mac(uint8_t mac[6]);

/**
 * @brief Render the key=value identity line for HelloReply (A-05).
 *
 * @param[out] out      Buffer; holds "no identity" when absent.
 * @param[in]  capacity Size of @p out, bytes.
 * @return true when rendered, false when absent.
 */
bool app_identity_detail(char *out, size_t capacity);

#endif /* IDENTITY_H */
