/**
 * @file    hwlink.c
 * @brief   HWLINK framing: 4-byte header, CRC-32 and COBS.
 * @details Unencoded frame: proto[1] flags[1] seq[2] payload[n] crc32[4],
 *          multi-byte fields little-endian.  The CRC covers everything before
 *          it; the 0x00 delimiter is not covered.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     HWLINK LNK-001..LNK-005 (SRS-GUI-F767 Appendix A)
 */

#include <string.h>
#include <zephyr/sys/crc.h>
#include "comms/hwlink.h"

/** Header size: proto, flags, seq. */
#define HWLINK_HEADER_SIZE   4U
/** CRC-32 field size, bytes. */
#define HWLINK_CRC_SIZE      4U
/** Header plus CRC, bytes. */
#define HWLINK_OVERHEAD      (HWLINK_HEADER_SIZE + HWLINK_CRC_SIZE)

/**
 * @brief Build an unencoded frame: proto, flags, seq, payload, CRC-32.
 *
 * @param[in]  payload      Payload bytes (non-NULL).
 * @param[in]  payload_len  Payload length, bytes.
 * @param[in]  flags        HWLINK_FLAG_* mask.
 * @param[in]  seq          Sequence number.
 * @param[out] raw          Frame buffer.
 * @param[in]  raw_capacity Size of @p raw, bytes.
 * @param[out] raw_len      Frame length, bytes.
 * @return true on success, false on a NULL argument or overflow.
 */
bool hwlink_build_raw(const uint8_t *payload, size_t payload_len,
		      uint8_t flags, uint16_t seq,
		      uint8_t *raw, size_t raw_capacity, size_t *raw_len)
{
	size_t total = HWLINK_OVERHEAD + payload_len;

	if (raw == NULL || payload == NULL || raw_len == NULL ||
	    total > raw_capacity || total > HWLINK_MAX_FRAME) {
		return false;
	}

	raw[0] = HWLINK_PROTOCOL_MAJOR;
	raw[1] = flags;
	raw[2] = (uint8_t)(seq & 0xFFU);
	raw[3] = (uint8_t)(seq >> 8U);
	memcpy(&raw[4], payload, payload_len);

	uint32_t crc = crc32_ieee(raw, HWLINK_HEADER_SIZE + payload_len);
	raw[HWLINK_HEADER_SIZE + payload_len + 0U] = (uint8_t)(crc & 0xFFU);
	raw[HWLINK_HEADER_SIZE + payload_len + 1U] = (uint8_t)((crc >> 8U) & 0xFFU);
	raw[HWLINK_HEADER_SIZE + payload_len + 2U] = (uint8_t)((crc >> 16U) & 0xFFU);
	raw[HWLINK_HEADER_SIZE + payload_len + 3U] = (uint8_t)((crc >> 24U) & 0xFFU);

	*raw_len = total;
	return true;
}

/**
 * @brief COBS-encode a frame and append the 0x00 delimiter.
 *
 * @param[in]  raw          Frame bytes.
 * @param[in]  raw_len      Frame length, bytes (> 0).
 * @param[out] out          Encoded buffer.
 * @param[in]  out_capacity Size of @p out, bytes.
 * @param[out] out_len      Encoded length including the delimiter.
 * @return true on success, false on a bad argument or overflow.
 */
bool hwlink_cobs_encode(const uint8_t *raw, size_t raw_len,
			uint8_t *out, size_t out_capacity, size_t *out_len)
{
	size_t read = 0U;
	size_t write = 0U;
	size_t code_pos = 0U;

	if (raw == NULL || out == NULL || out_len == NULL || raw_len == 0U) {
		return false;
	}
	if (out_capacity < 2U) {
		return false;
	}

	code_pos = write++;
	out[code_pos] = 1U;

	while (read < raw_len) {
		if (raw[read] == 0U) {
			out[code_pos] = (uint8_t)(write - code_pos);
			code_pos = write++;
			if (write >= out_capacity) {
				return false;
			}
			out[code_pos] = 1U;
			read++;
		} else {
			if (write >= out_capacity) {
				return false;
			}
			out[write++] = raw[read++];
			if ((uint8_t)(write - code_pos) == 0xFFU) {
				out[code_pos] = 0xFFU;
				code_pos = write++;
				if (write >= out_capacity) {
					return false;
				}
				out[code_pos] = 1U;
			}
		}
	}

	out[code_pos] = (uint8_t)(write - code_pos);
	if (write >= out_capacity) {
		return false;
	}
	out[write++] = 0U;   /* frame delimiter */

	*out_len = write;
	return true;
}

/**
 * @brief COBS-decode one frame body (delimiter removed).
 *
 * @param[in]  in           Encoded bytes.
 * @param[in]  in_len       Encoded length, bytes.
 * @param[out] out          Decoded buffer.
 * @param[in]  out_capacity Size of @p out, bytes.
 * @param[out] out_len      Decoded length, bytes.
 * @retval 0  Success.
 * @retval -1 Malformed input or overflow.
 */
int hwlink_cobs_decode(const uint8_t *in, size_t in_len,
		       uint8_t *out, size_t out_capacity, size_t *out_len)
{
	size_t read = 0U;
	size_t write = 0U;

	if (in == NULL || out == NULL || out_len == NULL) {
		return -1;
	}

	while (read < in_len) {
		uint8_t code = in[read++];
		if (code == 0U) {
			return -1;
		}

		size_t block = (size_t)code - 1U;
		if (read + block > in_len) {
			return -1;
		}
		if (write + block > out_capacity) {
			return -1;
		}
		memcpy(&out[write], &in[read], block);
		write += block;
		read += block;

		if ((code != 0xFFU) && (read < in_len)) {
			if (write >= out_capacity) {
				return -1;
			}
			out[write++] = 0U;
		}
	}

	*out_len = write;
	return 0;
}

/**
 * @brief Check a frame's proto byte, flags and CRC, and copy out the payload.
 *
 * @param[in]  raw              Frame bytes.
 * @param[in]  raw_len          Frame length, bytes.
 * @param[out] payload          Payload buffer.
 * @param[in]  payload_capacity Size of @p payload, bytes.
 * @param[out] payload_len      Payload length, bytes.
 * @param[out] flags            HWLINK_FLAG_* mask.
 * @param[out] seq              Sequence number.
 * @return true when the frame is valid.
 */
bool hwlink_decode_raw(const uint8_t *raw, size_t raw_len,
		       uint8_t *payload, size_t payload_capacity,
		       size_t *payload_len, uint8_t *flags, uint16_t *seq)
{
	if (raw == NULL || payload == NULL || payload_len == NULL ||
	    flags == NULL || seq == NULL) {
		return false;
	}
	if (raw_len < HWLINK_OVERHEAD || raw_len > HWLINK_MAX_FRAME) {
		return false;
	}
	if (raw[0] != HWLINK_PROTOCOL_MAJOR || (raw[1] & ~0x03U) != 0U) {
		return false;
	}

	size_t body_len = raw_len - HWLINK_CRC_SIZE;
	uint32_t expected = (uint32_t)raw[body_len] |
			    ((uint32_t)raw[body_len + 1U] << 8U) |
			    ((uint32_t)raw[body_len + 2U] << 16U) |
			    ((uint32_t)raw[body_len + 3U] << 24U);
	if (crc32_ieee(raw, body_len) != expected) {
		return false;
	}

	size_t len = body_len - HWLINK_HEADER_SIZE;
	if (len > payload_capacity) {
		return false;
	}
	memcpy(payload, &raw[HWLINK_HEADER_SIZE], len);

	*payload_len = len;
	*flags = raw[1];
	*seq = (uint16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8U));
	return true;
}
