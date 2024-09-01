/**
 * @file    hwlink.h
 * @brief   HWLINK framing: 4-byte header, CRC-32 and COBS.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     HWLINK LNK-001..LNK-005 (SRS-GUI-F767 Appendix A)
 */

#ifndef HWLINK_H
#define HWLINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HWLINK_PROTOCOL_MAJOR     1U    /**< Protocol major; also the proto byte. */
#define HWLINK_PROTOCOL_MINOR     1U    /**< Protocol minor. */
#define HWLINK_MAX_FRAME          2048U /**< Maximum unencoded frame, bytes. */
/** Maximum COBS-encoded frame: one code byte per 254, plus 2. */
#define HWLINK_MAX_ENCODED        (HWLINK_MAX_FRAME + (HWLINK_MAX_FRAME / 254U) + 2U)

#define HWLINK_FLAG_RESPONSE      (1U << 0U) /**< Frame is a response. */
#define HWLINK_FLAG_EVENT         (1U << 1U) /**< Frame is an unsolicited event. */

/**
 * @brief Build an unencoded frame: proto, flags, seq (LE), payload, CRC-32 (LE).
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
		      uint8_t *raw, size_t raw_capacity, size_t *raw_len);

/**
 * @brief COBS-encode a frame and append the 0x00 delimiter.
 *
 * @param[in]  raw          Frame bytes.
 * @param[in]  raw_len      Frame length, bytes (> 0).
 * @param[out] out          Encoded buffer, HWLINK_MAX_ENCODED for a full frame.
 * @param[in]  out_capacity Size of @p out, bytes.
 * @param[out] out_len      Encoded length including the delimiter.
 * @return true on success, false on a bad argument or overflow.
 */
bool hwlink_cobs_encode(const uint8_t *raw, size_t raw_len,
			uint8_t *out, size_t out_capacity, size_t *out_len);

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
		       uint8_t *out, size_t out_capacity, size_t *out_len);

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
		       size_t *payload_len, uint8_t *flags, uint16_t *seq);

#endif /* HWLINK_H */
