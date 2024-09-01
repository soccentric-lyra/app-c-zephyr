/**
 * @file    boot_contract.h
 * @brief   Flash layout, hand-over and metadata contract with BOOT.
 * @details Values mirror SRS-BOOT-F767 LAY-001..LAY-005, HND-003 and
 *          Appendix B.  The application only reads slot metadata and programs
 *          the one CONFIRMED record.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6, dual-bank flash (nDBANK = 0)
 * @srs     DBT-001, DBT-004, DBT-005, DBT-006, HLT-002, DSAFE-002
 */

#ifndef BOOT_CONTRACT_H
#define BOOT_CONTRACT_H

#include <stdint.h>

/* LAY-002: every application is linked at the slot offset in bank 1.  BOOT
 * maps the active bank at 0x08000000 with SYSCFG_MEMRMP.SWP_FB, so the same
 * image runs from slot A and slot B. */
#define APP_SLOT_BASE           0x0800C000UL /**< Application link address. */
#define APP_SLOT_SIZE           0x000F4000UL /**< Slot size, 976 KB. */

#define APP_BANK_SIZE           0x00100000UL /**< Flash bank size, 1 MB. */
#define APP_META_OFFSET         0x00008000UL /**< Metadata sector offset in a bank. */
#define APP_META_SIZE           0x00004000UL /**< Metadata sector size, 16 KB. */

/* Physical addresses without bank swap (what BOOT uses while it runs). */
#define APP_META_A_ADDR         0x08008000UL /**< Slot A metadata, physical. */
#define APP_META_B_ADDR         0x08108000UL /**< Slot B metadata, physical. */

/**
 * @brief Running slot's own metadata sector.
 * @details SWP_FB maps the running bank at 0x08000000, so this alias is
 *          valid from either slot.
 */
#define APP_RUNNING_META_ADDR   0x08008000UL

/* Appendix B record offsets and magic words (little-endian). */
#define APP_META_HEADER_OFFSET    0x000U /**< Header record offset. */
#define APP_META_VERIFIED_OFFSET  0x100U /**< VERIFIED record offset. */
#define APP_META_ACTIVATED_OFFSET 0x120U /**< ACTIVATED record offset. */
#define APP_META_CONFIRMED_OFFSET 0x140U /**< CONFIRMED record offset. */
#define APP_META_REJECTED_OFFSET  0x160U /**< REJECTED record offset. */
#define APP_META_RECORD_SIZE      32U    /**< State record size, bytes. */

#define APP_META_VERIFIED_MAGIC   0x56455249UL /**< "VERI". */
#define APP_META_ACTIVATED_MAGIC  0x41435456UL /**< "ACTV". */
#define APP_META_CONFIRMED_MAGIC  0x434F4E46UL /**< "CONF". */
#define APP_META_REJECTED_MAGIC   0x52454A45UL /**< "REJE". */

/** CONFIRMED record address for the running slot. */
#define APP_CONFIRMED_ADDR \
	(APP_RUNNING_META_ADDR + APP_META_CONFIRMED_OFFSET)

/* Hand-over word in RTC_BKP2R (HND-003), written by BOOT:
 *
 *   bits  0..23  magic 0x444E48 (the bytes "HND")
 *   bits 24..25  slot: 1 = A, 2 = B, 0 = unknown
 *   bit  26      trial pending
 *   bits 27..28  trial counter (0..3)
 *
 * The packing is private to the BOOT/application pair.  Without BOOT the
 * word reads as zero and the slot is UNKNOWN (DBT-004). */
#define APP_HANDOVER_MAGIC        0x00444E48UL /**< "HND" in bits 0..23. */
#define APP_HANDOVER_MAGIC_MASK   0x00FFFFFFUL /**< Magic field mask. */
#define APP_HANDOVER_SLOT_SHIFT   24U          /**< Slot field shift. */
#define APP_HANDOVER_SLOT_MASK    0x3UL        /**< Slot field mask, after shift. */
#define APP_HANDOVER_PENDING_BIT  26U          /**< Trial-pending bit number. */
#define APP_HANDOVER_TRIAL_SHIFT  27U          /**< Trial counter shift. */
#define APP_HANDOVER_TRIAL_MASK   0x3UL        /**< Trial counter mask, after shift. */

/** RTC_BKP1R value ("STAY") that keeps BOOT in the bootloader (ENT-002). */
#define APP_STAY_IN_BOOT_MAGIC    0x59415453UL

/* Slot identifiers shared with Status. */
#define APP_SLOT_UNKNOWN          0U /**< No valid hand-over record. */
#define APP_SLOT_A                1U /**< Slot A. */
#define APP_SLOT_B                2U /**< Slot B. */

#endif /* BOOT_CONTRACT_H */
