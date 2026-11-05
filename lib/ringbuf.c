#include "ringbuf.h"

#define RB_MASK (RINGBUF_SIZE - 1)

void ringbuf_init(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->overflows = 0;
}

int ringbuf_put(ringbuf_t *rb, uint8_t c)
{
    uint32_t next = (rb->head + 1) & RB_MASK;

    if (next == rb->tail) {
        rb->overflows++;
        return -1;
    }
    rb->buf[rb->head] = c;
    rb->head = next;   /* update head only after the byte is stored */
    return 0;
}

int ringbuf_get(ringbuf_t *rb, uint8_t *c)
{
    if (rb->tail == rb->head) {
        return -1;
    }
    *c = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & RB_MASK;
    return 0;
}

uint32_t ringbuf_count(const ringbuf_t *rb)
{
    return (rb->head - rb->tail) & RB_MASK;
}
