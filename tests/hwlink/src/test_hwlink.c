/*
 * test_hwlink.c - HWLINK framing tests on Zephyr native_sim
 *
 * Author:  Sandesh Ghimire <sandesh@soccentric.com>
 * Target:  native_sim (host), also usable under twister
 *
 * The suite links the application's src/comms/hwlink.c, so the COBS and CRC
 * code paths are the ones the firmware runs.  Zephyr supplies crc32_ieee(),
 * which is the same CRC-32/ISO-HDLC the linker script and BOOT use.
 */

#include <string.h>
#include <zephyr/sys/crc.h>
#include <zephyr/ztest.h>

#include "comms/hwlink.h"

static void roundtrip(const uint8_t *payload, size_t len)
{
	uint8_t raw[HWLINK_MAX_FRAME];
	uint8_t encoded[HWLINK_MAX_ENCODED];
	uint8_t decoded[HWLINK_MAX_FRAME];
	uint8_t out[HWLINK_MAX_FRAME];
	size_t raw_len = 0U;
	size_t encoded_len = 0U;
	size_t decoded_len = 0U;
	size_t out_len = 0U;
	uint8_t flags = 0U;
	uint16_t seq = 0U;

	zassert_true(hwlink_build_raw(payload, len, HWLINK_FLAG_RESPONSE,
				      0x1234U, raw, sizeof(raw), &raw_len),
		     "build_raw failed");
	zassert_equal(raw_len, 8U + len, "raw length");
	zassert_equal(raw[0], HWLINK_PROTOCOL_MAJOR, "proto byte");
	zassert_equal(raw[1], HWLINK_FLAG_RESPONSE, "flags byte");
	zassert_equal(raw[2], 0x34U, "seq low byte");
	zassert_equal(raw[3], 0x12U, "seq high byte");

	zassert_true(hwlink_cobs_encode(raw, raw_len, encoded,
					sizeof(encoded), &encoded_len),
		     "cobs_encode failed");
	zassert_equal(encoded[encoded_len - 1U], 0U, "delimiter");
	for (size_t i = 0U; i < encoded_len - 1U; i++) {
		zassert_not_equal(encoded[i], 0U, "embedded zero at %zu", i);
	}

	zassert_equal(hwlink_cobs_decode(encoded, encoded_len - 1U, decoded,
					 sizeof(decoded), &decoded_len),
		      0, "cobs_decode failed");
	zassert_equal(decoded_len, raw_len, "decoded length");
	zassert_mem_equal(decoded, raw, raw_len, "decoded frame");

	zassert_true(hwlink_decode_raw(decoded, decoded_len, out, sizeof(out),
				       &out_len, &flags, &seq),
		     "decode_raw failed");
	zassert_equal(out_len, len, "payload length");
	zassert_equal(flags, HWLINK_FLAG_RESPONSE, "decoded flags");
	zassert_equal(seq, 0x1234U, "decoded seq");
	zassert_mem_equal(out, payload, len, "payload");
}

ZTEST(hwlink, test_crc_vector)
{
	zassert_equal(crc32_ieee((const uint8_t *)"123456789", 9U),
		      0xCBF43926UL, "CRC-32/ISO-HDLC check value");
}

ZTEST(hwlink, test_roundtrip)
{
	static const uint8_t payload[] = {0x00, 0x01, 0x02, 0xFE, 0xFF, 0x00};

	roundtrip((const uint8_t *)"", 0U);
	roundtrip(payload, sizeof(payload));
}

ZTEST(hwlink, test_cobs_examples)
{
	static const uint8_t zero[] = {0x00};
	static const uint8_t spread[] = {0x01, 0x00, 0x02};
	uint8_t buf[16];
	size_t n = 0U;

	zassert_true(hwlink_cobs_encode(zero, sizeof(zero), buf, sizeof(buf),
					&n), "encode zero");
	zassert_equal(n, 3U, "encoded zero length");
	zassert_equal(buf[0], 0x01U, "code byte");
	zassert_equal(buf[1], 0x01U, "implicit zero block");
	zassert_equal(buf[2], 0x00U, "delimiter");

	zassert_true(hwlink_cobs_encode(spread, sizeof(spread), buf, sizeof(buf),
					&n), "encode spread");
	zassert_equal(n, 5U, "encoded spread length");
	zassert_equal(buf[0], 0x02U, "first code");
	zassert_equal(buf[2], 0x02U, "second code");
}

ZTEST(hwlink, test_cobs_boundaries)
{
	uint8_t big[HWLINK_MAX_FRAME];
	uint8_t encoded[HWLINK_MAX_ENCODED];
	uint8_t decoded[HWLINK_MAX_FRAME];
	size_t encoded_len = 0U;
	size_t decoded_len = 0U;

	for (size_t len = 253U; len <= 255U; len++) {
		memset(big, 0x5A, len);
		zassert_true(hwlink_cobs_encode(big, len, encoded,
						sizeof(encoded), &encoded_len),
			     "encode %zu bytes", len);
		zassert_equal(encoded[encoded_len - 1U], 0U, "delimiter");
		zassert_equal(hwlink_cobs_decode(encoded, encoded_len - 1U,
						 decoded, sizeof(decoded),
						 &decoded_len),
			      0, "decode %zu bytes", len);
		zassert_equal(decoded_len, len, "decoded length");
		zassert_mem_equal(decoded, big, len, "decoded bytes");
	}
}

ZTEST(hwlink, test_rejects_bad_frames)
{
	uint8_t raw[HWLINK_MAX_FRAME];
	uint8_t out[8];
	uint8_t flags = 0U;
	uint16_t seq = 0U;
	size_t raw_len = 0U;
	size_t out_len = 0U;
	static const uint8_t payload[] = {0x01, 0x02, 0x03};

	zassert_equal(hwlink_cobs_decode((const uint8_t *)"\x00", 1U, out,
					 sizeof(out), &out_len),
		      -1, "embedded zero code");
	zassert_equal(hwlink_cobs_decode((const uint8_t *)"\x03\x01", 2U, out,
					 sizeof(out), &out_len),
		      -1, "truncated block");

	zassert_true(hwlink_build_raw(payload, sizeof(payload), 0U, 7U, raw,
				      sizeof(raw), &raw_len), "build frame");
	raw[raw_len - 1U] ^= 0xFFU;
	out_len = 0xDEADBEEFU;
	zassert_false(hwlink_decode_raw(raw, raw_len, out, sizeof(out),
					&out_len, &flags, &seq),
		      "bad CRC accepted");
	zassert_equal(out_len, 0xDEADBEEFU, "out_len changed on failure");

	zassert_true(hwlink_build_raw(payload, sizeof(payload), 0x04U, 7U, raw,
				      sizeof(raw), &raw_len), "build flags");
	zassert_false(hwlink_decode_raw(raw, raw_len, out, sizeof(out),
					&out_len, &flags, &seq),
		      "reserved flag accepted");
}

ZTEST_SUITE(hwlink, NULL, NULL, NULL, NULL, NULL);
