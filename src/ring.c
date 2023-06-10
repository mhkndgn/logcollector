#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>
#include <malloc.h>
#include <stdlib.h>

#include "ring.h"

/* return the size of memory occupied by a ring */
ssize_t ring_get_memsize(unsigned count)
{
    ssize_t sz;

    /* count must be a power of 2 */
    if((!POWEROF2(count)) || (count > RING_SZ_MASK))
        FATAL("Requested size is invalid, must be power of 2, and do not exceed the size limit %u", RING_SZ_MASK);
//    fatal_log_if((!POWEROF2(count)) || (count > RING_SZ_MASK),
//            "Requested size is invalid, must be power of 2, and do not exceed the size limit %u", RING_SZ_MASK);

    sz = sizeof(struct ring) + count * sizeof(void *);
    sz = ALIGN(sz, CACHE_LINE_SIZE);
    return sz;
}

static inline uint32_t align32pow2(uint32_t x)
{
    x--;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return x + 1;
}

int ring_init(struct ring *r, const char *name, unsigned count, unsigned flags)
{
    /* compilation-time checks */
    BUILD_BUG_ON((sizeof(struct ring) & CACHE_LINE_MASK) != 0);
    BUILD_BUG_ON((offsetof(struct ring, cons) & CACHE_LINE_MASK) != 0);
    BUILD_BUG_ON((offsetof(struct ring, prod) & CACHE_LINE_MASK) != 0);

    /* init the ring structure */
    memset(r, 0, sizeof(*r));
    r->flags = flags;

    strncpy(r->name, name, sizeof(r->name));

    if (flags & RING_F_EXACT_SZ) {
        r->size = align32pow2(count + 1);
        r->mask = r->size - 1;
        r->capacity = count;
    } else {
        //fatal_log_if((!POWEROF2(count)) || (count > RING_SZ_MASK),
            //    "Requested size is invalid, must be power of 2, and not exceed the size limit %u", RING_SZ_MASK);
        if((!POWEROF2(count)) || (count > RING_SZ_MASK))
            FATAL("Requested size is invalid, must be power of 2, and do not exceed the size limit %u", RING_SZ_MASK);

        r->size = count;
        r->mask = count - 1;
        r->capacity = r->mask;
    }
    r->prod.head = r->cons.head = 0;
    r->prod.tail = r->cons.tail = 0;

    return 0;
}


/* create the ring */
struct ring *ring_create(char *name, unsigned count, unsigned flags)
{
    struct ring *r = NULL;

    if (!name)
        FATAL("Ring name cannot be null or empty!");

    const unsigned int requested_count = count;

    /* for an exact size ring, round up from count to a power of two */
    if (flags & RING_F_EXACT_SZ)
        count = align32pow2(count + 1);

    ssize_t ring_size = ring_get_memsize(count);
    if(ring_size < 0)
        FATAL("ring_get_memsize error %zd", ring_size);

    int ret = posix_memalign((void **) &r, 4096, ring_size);
    if(ret != 0)
        FATAL("posix_memalign");

    ring_init(r, name, requested_count, flags);

    return r;
}

/* free the ring */
void ring_free(struct ring *r)
{
    free(r);
}

/* dump the status of the ring on the console */
void ring_dump(FILE *f, const struct ring *r)
{
    fprintf(f, "ring @%p\n", r);
    fprintf(f, "  flags=%x\n", r->flags);
    fprintf(f, "  size=%"PRIu32"\n", r->size);
    fprintf(f, "  capacity=%"PRIu32"\n", r->capacity);
    fprintf(f, "  ct=%"PRIu32"\n", r->cons.tail);
    fprintf(f, "  ch=%"PRIu32"\n", r->cons.head);
    fprintf(f, "  pt=%"PRIu32"\n", r->prod.tail);
    fprintf(f, "  ph=%"PRIu32"\n", r->prod.head);
    fprintf(f, "  used=%u\n", ring_count(r));
    fprintf(f, "  avail=%u\n", ring_free_count(r));
}

