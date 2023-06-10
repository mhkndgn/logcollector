#ifndef _RING_C11_MEM_H_
#define _RING_C11_MEM_H_

#include "defines.h"

static __always_inline void
update_tail(struct ring_headtail *ht, uint32_t old_val, uint32_t new_val,
		uint32_t single, uint32_t enqueue)
{
	(void)(enqueue);

	__atomic_store_n(&ht->tail, new_val, __ATOMIC_RELEASE);
}

/**
 * @internal This function updates the producer head for enqueue
 *
 * @param r
 *   A pointer to the ring structure
 * @param is_sp
 *   Indicates whether multi-producer path is needed or not
 * @param n
 *   The number of elements we will want to enqueue, i.e. how far should the
 *   head be moved
 * @param behavior
 *   RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   RING_QUEUE_VARIABLE: Enqueue as many items as possible from ring
 * @param old_head
 *   Returns head value as it was before the move, i.e. where enqueue starts
 * @param new_head
 *   Returns the current/new head value i.e. where enqueue finishes
 * @param free_entries
 *   Returns the amount of free space in the ring BEFORE head was moved
 * @return
 *   Actual number of objects enqueued.
 *   If behavior == RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __always_inline unsigned int
__ring_move_prod_head(struct ring *r, unsigned int is_sp,
		unsigned int n, enum ring_queue_behavior behavior,
		uint32_t *old_head, uint32_t *new_head,
		uint32_t *free_entries)
{
	const uint32_t capacity = r->capacity;
	uint32_t cons_tail;
	unsigned int max = n;

	*old_head = __atomic_load_n(&r->prod.head, __ATOMIC_RELAXED);
    /* Reset n to the initial burst count */
    n = max;

    /* Ensure the head is read before tail */
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    /* load-acquire synchronize with store-release of ht->tail
     * in update_tail.
     */
    cons_tail = __atomic_load_n(&r->cons.tail,
                __ATOMIC_ACQUIRE);

    /* The subtraction is done between two unsigned 32bits value
     * (the result is always modulo 32 bits even if we have
     * *old_head > cons_tail). So 'free_entries' is always between 0
     * and capacity (which is < size).
     */
    *free_entries = (capacity + cons_tail - *old_head);

    /* check that we have enough room in ring */
    if (unlikely(n > *free_entries))
        n = (behavior == RING_QUEUE_FIXED) ?
                0 : *free_entries;

    if (n == 0)
        return 0;

    *new_head = *old_head + n;

    r->prod.head = *new_head;

	return n;
}

/**
 * @internal This function updates the consumer head for dequeue
 *
 * @param r
 *   A pointer to the ring structure
 * @param is_sc
 *   Indicates whether multi-consumer path is needed or not
 * @param n
 *   The number of elements we will want to enqueue, i.e. how far should the
 *   head be moved
 * @param behavior
 *   RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 * @param old_head
 *   Returns head value as it was before the move, i.e. where dequeue starts
 * @param new_head
 *   Returns the current/new head value i.e. where dequeue finishes
 * @param entries
 *   Returns the number of entries in the ring BEFORE head was moved
 * @return
 *   - Actual number of objects dequeued.
 *     If behavior == RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __always_inline unsigned int
__ring_move_cons_head(struct ring *r, int is_sc,
		unsigned int n, enum ring_queue_behavior behavior,
		uint32_t *old_head, uint32_t *new_head,
		uint32_t *entries)
{
	unsigned int max = n;
	uint32_t prod_tail;

	/* move cons.head atomically */
	*old_head = __atomic_load_n(&r->cons.head, __ATOMIC_RELAXED);

	/* Restore n as it may change every loop */
    n = max;

    /* Ensure the head is read before tail */
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    /* this load-acquire synchronize with store-release of ht->tail
     * in update_tail.
     */
    prod_tail = __atomic_load_n(&r->prod.tail,
                __ATOMIC_ACQUIRE);

    /* The subtraction is done between two unsigned 32bits value
     * (the result is always modulo 32 bits even if we have
     * cons_head > prod_tail). So 'entries' is always between 0
     * and size(ring)-1.
     */
    *entries = (prod_tail - *old_head);

    /* Set the actual entries for dequeue */
    if (n > *entries)
        n = (behavior == RING_QUEUE_FIXED) ? 0 : *entries;

    if (unlikely(n == 0))
        return 0;

    *new_head = *old_head + n;

    r->cons.head = *new_head;

    return n;
}

#endif /* _RING_C11_MEM_H_ */
