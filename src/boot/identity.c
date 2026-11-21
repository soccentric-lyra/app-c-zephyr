/**
 * @file    identity.c
 * @brief   Factory PRV1 identity record reader (A-03).
 * @details The factory writes PRV1 into bank 2 sector 13 (0x08104000, or
 *          0x08004000 while bank 2 is swapped in).  The header, both CRC-32
 *          values and the chip UID binding are checked; a blank or torn
 *          record reads as absent.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-04
 * @target  STM32F767ZIT6
 * @srs     identity contract PRV1, factory A-02/A-03/A-05/A-06
 */

#include <string.h>
#include <soc.h>
#include <zephyr/sys/crc.h>
#include "boot/identity.h"

#define ID_MAGIC              0x31565250UL /**< "PRV1", little-endian. */
#define ID_FORMAT             1U           /**< Supported record format. */
#define ID_HEADER_SIZE        32U          /**< Header size, bytes. */
#define ID_MAX_TLV            4096U        /**< Maximum TLV body, bytes. */

#define ID_T_SERIAL           0x01U /**< TLV: serial number. */
#define ID_T_PRODUCT          0x02U /**< TLV: product name. */
#define ID_T_HW_REV           0x03U /**< TLV: hardware revision. */
#define ID_T_MFG_TIME         0x04U /**< TLV: manufacturing time (ignored). */
#define ID_T_STATION_FIXTURE  0x05U /**< TLV: station/fixture (ignored). */
#define ID_T_PROFILE          0x06U /**< TLV: provisioning profile. */
#define ID_T_FIRMWARE_VERSION 0x07U /**< TLV: firmware at provisioning (ignored). */
#define ID_T_MAC              0x10U /**< TLV: EUI-48 MAC, 6 bytes. */

#define ID_IDENTITY_A         0x08104000UL /**< Bank 2 sector 13, physical. */
#define ID_IDENTITY_SWAPPED   0x08004000UL /**< Same sector, bank 2 swapped in. */

/**
 * @brief PRV1 header, 32 bytes.
 * @note Layout is fixed by the factory provisioning contract.
 */
typedef struct {
	uint32_t magic;        /**< ID_MAGIC. */
	uint16_t format;       /**< ID_FORMAT. */
	uint16_t revision;     /**< Record revision. */
	uint32_t length;       /**< TLV body length, bytes (2..ID_MAX_TLV). */
	uint32_t tlv_crc32;    /**< CRC-32 of the TLV body. */
	uint8_t uid[12];       /**< Chip UID the record is bound to. */
	uint32_t header_crc32; /**< CRC-32 of bytes 0..27. */
} id_header_t;

/**
 * @brief Decoded identity fields.
 */
typedef struct {
	bool valid;           /**< true when the record passed all checks. */
	uint16_t revision;    /**< Record revision. */
	char serial[33];      /**< Serial number, NUL-terminated. */
	char product[33];     /**< Product name, NUL-terminated. */
	char hw_revision[9];  /**< Hardware revision, NUL-terminated. */
	char profile[65];     /**< Provisioning profile, NUL-terminated. */
	uint8_t mac[6];       /**< EUI-48 MAC. */
	bool mac_valid;       /**< true when mac is set. */
} app_identity_t;

/** Cached record; valid only when identity.valid is set. */
static app_identity_t identity;
/** true once app_identity_refresh() has run. */
static bool refreshed;

/**
 * @brief CRC-32/ISO-HDLC of header bytes 0..27.
 *
 * @param[in] raw 32-byte header.
 * @return CRC-32.
 */
static uint32_t header_crc(const uint8_t *raw)
{
	return crc32_ieee(raw, 28U);
}

/**
 * @brief Copy a TLV text field, truncating and NUL-terminating.
 *
 * @param[out] out    Destination.
 * @param[in]  cap    Size of @p out, bytes (> 0).
 * @param[in]  data   Source bytes.
 * @param[in]  length Source length, bytes.
 */
static void copy_text(char *out, size_t cap, const uint8_t *data, uint8_t length)
{
	size_t n = length;
	if (n >= cap) {
		n = cap - 1U;
	}
	memcpy(out, data, n);
	out[n] = '\0';
}

/**
 * @brief Validate and decode the PRV1 record at @p address into @ref identity.
 * @details Checks magic, format, length, header CRC, UID binding and TLV CRC.
 *          A type-0 TLV ends the scan.  Serial and product are mandatory.
 *
 * @param[in] address Record flash address.
 * @return true when a complete, valid record was decoded.
 */
static bool parse_from(uint32_t address)
{
	uint8_t raw[ID_HEADER_SIZE];

	SCB_InvalidateDCache_by_Addr((uint32_t *)address, ID_MAX_TLV);
	memcpy(raw, (const void *)address, sizeof(raw));

	id_header_t header;
	memcpy(&header, raw, sizeof(header));
	if (header.magic != ID_MAGIC) {
		return false;
	}
	if (header.format != ID_FORMAT || header.length == 0U ||
	    header.length > ID_MAX_TLV || header.length < 2U) {
		return false;
	}
	if (header_crc(raw) != header.header_crc32) {
		return false;
	}

	/* UID binding: the record has the three UID words in flash byte order. */
	uint8_t uid[12];
	const uint32_t uid_words[3] = {
		*(const volatile uint32_t *)UID_BASE,
		*(const volatile uint32_t *)(UID_BASE + 4U),
		*(const volatile uint32_t *)(UID_BASE + 8U),
	};
	memcpy(uid, uid_words, sizeof(uid));
	if (memcmp(uid, header.uid, sizeof(uid)) != 0) {
		return false;
	}

	const uint8_t *tlv = (const uint8_t *)(address + ID_HEADER_SIZE);
	if (crc32_ieee(tlv, header.length) != header.tlv_crc32) {
		return false;
	}

	memset(&identity, 0, sizeof(identity));
	identity.revision = header.revision;
	size_t offset = 0U;
	while (offset + 2U <= header.length) {
		uint8_t type = tlv[offset];
		uint8_t length = tlv[offset + 1U];
		offset += 2U;
		if (type == 0U) {
			break;
		}
		if (offset + length > header.length) {
			return false;
		}
		const uint8_t *value = &tlv[offset];
		switch (type) {
		case ID_T_SERIAL:
			copy_text(identity.serial, sizeof(identity.serial), value, length);
			break;
		case ID_T_PRODUCT:
			copy_text(identity.product, sizeof(identity.product), value, length);
			break;
		case ID_T_HW_REV:
			copy_text(identity.hw_revision, sizeof(identity.hw_revision), value,
				  length);
			break;
		case ID_T_PROFILE:
			copy_text(identity.profile, sizeof(identity.profile), value, length);
			break;
		case ID_T_MAC:
			if (length == 6U) {
				memcpy(identity.mac, value, 6U);
				identity.mac_valid = true;
			}
			break;
		default:
			break;
		}
		offset += length;
	}

	if (identity.serial[0] == '\0' || identity.product[0] == '\0') {
		return false;
	}
	identity.valid = true;
	return true;
}

/**
 * @brief Reload PRV1: physical bank-2 address first, then the swapped alias.
 *
 * @return true when a valid record was found.
 */
bool app_identity_refresh(void)
{
	refreshed = true;
	memset(&identity, 0, sizeof(identity));
	if (parse_from(ID_IDENTITY_A)) {
		return true;
	}
	return parse_from(ID_IDENTITY_SWAPPED);
}

/**
 * @brief Load the record on first use.
 *
 * @return true when a valid record is cached.
 */
static bool ensure_loaded(void)
{
	if (!refreshed) {
		(void)app_identity_refresh();
	}
	return identity.valid;
}

/**
 * @brief Report whether a valid record is present (loads on first use).
 *
 * @return true when valid.
 */
bool app_identity_valid(void)
{
	return ensure_loaded();
}

/**
 * @brief Return the serial number.
 *
 * @return NUL-terminated serial; "" when absent.
 */
const char *app_identity_serial(void)
{
	return ensure_loaded() ? identity.serial : "";
}

/**
 * @brief Return the product name.
 *
 * @return NUL-terminated name; "" when absent.
 */
const char *app_identity_product(void)
{
	return ensure_loaded() ? identity.product : "";
}

/**
 * @brief Return the hardware revision.
 *
 * @return NUL-terminated revision; "" when absent.
 */
const char *app_identity_hw_revision(void)
{
	return ensure_loaded() ? identity.hw_revision : "";
}

/**
 * @brief Return the provisioning profile.
 *
 * @return NUL-terminated profile; "" when absent.
 */
const char *app_identity_profile(void)
{
	return ensure_loaded() ? identity.profile : "";
}

/**
 * @brief Return the record revision.
 *
 * @return Revision; 0 when absent.
 */
uint16_t app_identity_revision(void)
{
	return ensure_loaded() ? identity.revision : 0U;
}

/**
 * @brief Copy the EUI-48 MAC.
 *
 * @param[out] mac 6-byte buffer.
 * @return true on success, false when no MAC is recorded.
 */
bool app_identity_mac(uint8_t mac[6])
{
	if (!ensure_loaded() || !identity.mac_valid) {
		return false;
	}
	memcpy(mac, identity.mac, 6U);
	return true;
}

/**
 * @brief Append a string, truncating to fit; always NUL-terminates.
 *
 * @param[out] out  Buffer.
 * @param[in]  cap  Size of @p out, bytes.
 * @param[in]  pos  Write offset.
 * @param[in]  text String; NULL appends nothing.
 * @return New write offset.
 */
static size_t append_text(char *out, size_t cap, size_t pos, const char *text)
{
	while (text != NULL && *text != '\0' && pos + 1U < cap) {
		out[pos++] = *text++;
	}
	out[pos] = '\0';
	return pos;
}

/**
 * @brief Append an unsigned decimal, truncating to fit; always NUL-terminates.
 *
 * @param[out] out   Buffer.
 * @param[in]  cap   Size of @p out, bytes.
 * @param[in]  pos   Write offset.
 * @param[in]  value Value.
 * @return New write offset.
 */
static size_t append_uint(char *out, size_t cap, size_t pos, unsigned value)
{
	char digits[10];
	size_t count = 0U;
	do {
		digits[count++] = (char)('0' + (value % 10U));
		value /= 10U;
	} while (value != 0U && count < sizeof(digits));
	while (count > 0U && pos + 1U < cap) {
		out[pos++] = digits[--count];
	}
	out[pos] = '\0';
	return pos;
}

/**
 * @brief Append the cached MAC as XX:XX:XX:XX:XX:XX; always NUL-terminates.
 *
 * @param[out] out Buffer.
 * @param[in]  cap Size of @p out, bytes.
 * @param[in]  pos Write offset.
 * @return New write offset.
 */
static size_t append_mac(char *out, size_t cap, size_t pos)
{
	static const char hex[] = "0123456789ABCDEF";
	for (size_t i = 0U; i < 6U && pos + 3U < cap; i++) {
		if (i != 0U) {
			out[pos++] = ':';
		}
		out[pos++] = hex[(identity.mac[i] >> 4U) & 0xFU];
		out[pos++] = hex[identity.mac[i] & 0xFU];
	}
	out[pos] = '\0';
	return pos;
}

/**
 * @brief Render "PRV1 serial=..;mac=..;hw=..;product=..;profile=..;revision=..".
 *
 * @param[out] out      Buffer; holds "no identity" when absent.
 * @param[in]  capacity Size of @p out, bytes.
 * @return true when rendered, false when absent or @p out is unusable.
 */
bool app_identity_detail(char *out, size_t capacity)
{
	if (out == NULL || capacity == 0U) {
		return false;
	}
	out[0] = '\0';
	if (!ensure_loaded()) {
		(void)append_text(out, capacity, 0U, "no identity");
		return false;
	}
	size_t pos = 0U;
	pos = append_text(out, capacity, pos, "PRV1 serial=");
	pos = append_text(out, capacity, pos, identity.serial);
	pos = append_text(out, capacity, pos, ";mac=");
	if (identity.mac_valid) {
		pos = append_mac(out, capacity, pos);
	}
	pos = append_text(out, capacity, pos, ";hw=");
	pos = append_text(out, capacity, pos, identity.hw_revision);
	pos = append_text(out, capacity, pos, ";product=");
	pos = append_text(out, capacity, pos, identity.product);
	pos = append_text(out, capacity, pos, ";profile=");
	pos = append_text(out, capacity, pos, identity.profile);
	pos = append_text(out, capacity, pos, ";revision=");
	(void)append_uint(out, capacity, pos, identity.revision);
	return true;
}
