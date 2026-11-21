/**
 * @file    flatcc_port.c
 * @brief   Fixed-arena first-fit allocator for the flatcc runtime.
 * @details 16-byte blocks in a .bss arena; malloc/free are never linked.
 * @author  Sandesh Ghimire <sandesh@soccentric.com>
 * @date    2026-11-03
 * @target  STM32F767ZIT6
 * @srs     DRT-004 (no allocation after start-up), DCON-002
 * @note    Unlocked: only the Comms thread uses flatcc.
 */

#include <string.h>
#include "comms/flatcc_port.h"

/** Arena size, bytes (overridable). */
#ifndef APP_FLATCC_HEAP_SIZE
#define APP_FLATCC_HEAP_SIZE (16U * 1024U)
#endif

/** Block and payload alignment, bytes. */
#define FLATCC_ALIGNMENT 16U

/**
 * @brief Arena block header; the payload follows it.
 */
typedef struct flatcc_block {
	size_t size;               /**< Payload size, bytes. */
	struct flatcc_block *next; /**< Next block in address order, or NULL. */
	uint32_t free;             /**< 1 when free, 0 when allocated. */
	uint32_t pad;              /**< Unused. */
} __attribute__((aligned(FLATCC_ALIGNMENT))) flatcc_block_t;

/**
 * @brief Arena storage, aligned to FLATCC_ALIGNMENT.
 */
typedef union {
	/* cppcheck-suppress unusedStructMember ; alignment only */
	long double align;
	/* cppcheck-suppress unusedStructMember ; alignment only */
	void *pointer;
	unsigned char bytes[APP_FLATCC_HEAP_SIZE]; /**< Arena bytes. */
} __attribute__((aligned(FLATCC_ALIGNMENT))) flatcc_arena_t;

/** Arena backing all flatcc allocations. */
static flatcc_arena_t flatcc_arena;
/** First block; NULL until the first allocation. */
static flatcc_block_t *flatcc_head;

/**
 * @brief Round up to FLATCC_ALIGNMENT.
 *
 * @param[in] n Size, bytes.
 * @return Rounded size, bytes.
 */
static size_t flatcc_align(size_t n)
{
	return (n + (FLATCC_ALIGNMENT - 1U)) & ~(size_t)(FLATCC_ALIGNMENT - 1U);
}

/**
 * @brief Make the whole arena one free block on first use.
 */
static void flatcc_heap_init(void)
{
	if (flatcc_head == NULL) {
		flatcc_head = (flatcc_block_t *)flatcc_arena.bytes;
		flatcc_head->size = APP_FLATCC_HEAP_SIZE - sizeof(flatcc_block_t);
		flatcc_head->next = NULL;
		flatcc_head->free = 1U;
	}
}

/**
 * @brief First-fit allocation; splits a block when the rest holds a header
 *        plus 16 bytes.
 *
 * @param[in] size Size, bytes.
 * @return Block, or NULL when @p size is 0 or nothing fits.
 */
void *app_flatcc_alloc(size_t size)
{
	flatcc_heap_init();
	if (size == 0U) {
		return NULL;
	}
	size = flatcc_align(size);

	for (flatcc_block_t *block = flatcc_head; block != NULL;
		 block = block->next) {
		if (block->free == 0U || block->size < size) {
			continue;
		}
		/* Split when the tail can hold a header and 16 payload bytes. */
		if ((block->size - size) >= (sizeof(flatcc_block_t) +
					     FLATCC_ALIGNMENT)) {
			flatcc_block_t *tail = (flatcc_block_t *)((unsigned char *)block +
								 sizeof(flatcc_block_t) +
								 size);
			tail->size = block->size - size - sizeof(flatcc_block_t);
			tail->next = block->next;
			tail->free = 1U;
			block->next = tail;
			block->size = size;
		}
		block->free = 0U;
		return (unsigned char *)block + sizeof(flatcc_block_t);
	}
	return NULL;
}

/**
 * @brief Free a block and merge it with free neighbours.
 *
 * @param[in] ptr Arena block, or NULL (no-op).
 */
void app_flatcc_free(void *ptr)
{
	if (ptr == NULL) {
		return;
	}
	flatcc_block_t *block = (flatcc_block_t *)((unsigned char *)ptr -
						   sizeof(flatcc_block_t));
	block->free = 1U;

	/* Coalesce with a free predecessor, then with free followers. */
	flatcc_block_t *prev = flatcc_head;
	while (prev != NULL && prev->next != block) {
		prev = prev->next;
	}
	if (prev != NULL && prev->free != 0U) {
		prev->size += sizeof(flatcc_block_t) + block->size;
		prev->next = block->next;
		block = prev;
	}
	while (block->next != NULL && block->next->free != 0U) {
		flatcc_block_t *next = block->next;
		block->size += sizeof(flatcc_block_t) + next->size;
		block->next = next->next;
	}
}

/**
 * @brief Resize a block.
 * @details Returns @p ptr unchanged when it is already large enough; else
 *          allocates, copies and frees the old block.
 *
 * @param[in] ptr  Arena block, or NULL to allocate.
 * @param[in] size New size, bytes; 0 frees @p ptr.
 * @return Block, or NULL on failure (old block kept) or @p size 0.
 */
void *app_flatcc_realloc(void *ptr, size_t size)
{
	if (ptr == NULL) {
		return app_flatcc_alloc(size);
	}
	if (size == 0U) {
		app_flatcc_free(ptr);
		return NULL;
	}

	const flatcc_block_t *block = (const flatcc_block_t *)(
		(unsigned char *)ptr - sizeof(flatcc_block_t));
	if (block->size >= flatcc_align(size)) {
		return ptr;
	}

	void *new_ptr = app_flatcc_alloc(size);
	if (new_ptr == NULL) {
		return NULL;
	}
	memcpy(new_ptr, ptr, block->size);
	app_flatcc_free(ptr);
	return new_ptr;
}

/**
 * @brief Allocate zeroed memory, checking count * size for overflow.
 *
 * @param[in] count Element count.
 * @param[in] size  Element size, bytes.
 * @return Block, or NULL on overflow or exhaustion.
 */
void *app_flatcc_calloc(size_t count, size_t size)
{
	if (count != 0U && size > (SIZE_MAX / count)) {
		return NULL;
	}
	size_t total = count * size;
	void *ptr = app_flatcc_alloc(total);
	if (ptr != NULL) {
		memset(ptr, 0, total);
	}
	return ptr;
}

/**
 * @brief Allocate a block; every block is already 16-byte aligned.
 *
 * @param[in] alignment Ignored.
 * @param[in] size      Size, bytes.
 * @return Block, or NULL on exhaustion.
 */
void *app_flatcc_aligned_alloc(size_t alignment, size_t size)
{
	(void)alignment;
	return app_flatcc_alloc(size);
}

/**
 * @brief Free a block from app_flatcc_aligned_alloc().
 *
 * @param[in] ptr Arena block, or NULL (no-op).
 */
void app_flatcc_aligned_free(void *ptr)
{
	app_flatcc_free(ptr);
}
