/**
 * @file    flatcc_port.h
 * @brief   flatcc allocation hooks backed by a fixed static arena.
 * @details CMakeLists.txt maps FLATCC_ALLOC/FLATCC_FREE/... to these
 *          functions and force-includes this header into every translation
 *          unit.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DRT-004 (no allocation after start-up), DCON-002 (flatcc runtime)
 * @note    Not thread-safe; only the Comms thread uses flatcc.
 */

#ifndef FLATCC_PORT_H
#define FLATCC_PORT_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Allocate from the arena (16-byte aligned).
 *
 * @param[in] size Size, bytes.
 * @return Block, or NULL when @p size is 0 or the arena is exhausted.
 */
void *app_flatcc_alloc(size_t size);

/**
 * @brief Return a block to the arena.
 *
 * @param[in] ptr Arena block, or NULL (no-op).
 */
void app_flatcc_free(void *ptr);

/**
 * @brief Resize an arena block.
 *
 * @param[in] ptr  Arena block, or NULL to allocate.
 * @param[in] size New size, bytes; 0 frees @p ptr.
 * @return Block (possibly moved), or NULL on failure or @p size 0.
 */
void *app_flatcc_realloc(void *ptr, size_t size);

/**
 * @brief Allocate @p count * @p size zeroed bytes from the arena.
 *
 * @param[in] count Element count.
 * @param[in] size  Element size, bytes.
 * @return Block, or NULL on overflow or exhaustion.
 */
void *app_flatcc_calloc(size_t count, size_t size);

/**
 * @brief Allocate an aligned block from the arena.
 *
 * @param[in] alignment Alignment, bytes; up to 16 is honoured.
 * @param[in] size      Size, bytes.
 * @return Block, or NULL on exhaustion.
 */
void *app_flatcc_aligned_alloc(size_t alignment, size_t size);

/**
 * @brief Free a block from app_flatcc_aligned_alloc().
 *
 * @param[in] ptr Arena block, or NULL (no-op).
 */
void app_flatcc_aligned_free(void *ptr);

#endif /* FLATCC_PORT_H */
