#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>

/* Single producer / single consumer byte ring buffer.
 * Producer = UART RX interrupt, consumer = command task.
 * head is only written by the producer and tail only by the consumer,
 * so no lock is needed (one slot is kept empty to tell full from empty).
 * RINGBUF_SIZE must be a power of 2. */
#define RINGBUF_SIZE 128

typedef struct {
    volatile uint8_t buf[RINGBUF_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t overflows;
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb);
int ringbuf_put(ringbuf_t *rb, uint8_t c);   /* 0 ok, -1 full (byte lost) */
int ringbuf_get(ringbuf_t *rb, uint8_t *c);  /* 0 ok, -1 empty */
uint32_t ringbuf_count(const ringbuf_t *rb);

#endif
