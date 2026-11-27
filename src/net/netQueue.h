/*
 * Single-producer, single-consumer ring of fixed-size Ethernet frame slots.
 * One side may be an interrupt handler. No locks: the producer writes only
 * `head`, the consumer writes only `tail`. Capacity must be a power of two
 * (masks, not division: the divider routines are in flash).
 */
#ifndef NET_QUEUE_H
#define NET_QUEUE_H

#include <stdint.h>
#include <string.h>

#define NETQ_FRAME_MAX 1514
/* A whole large card block (2048), so card transfers can go straight
 * into or out of a slot. */
#define NETQ_SLOT_BYTES 2048

typedef struct __attribute__((aligned(4)))
{
    uint16_t length;
    uint16_t reserved;
    uint8_t data[NETQ_SLOT_BYTES];
} netq_slot_t;

typedef struct
{
    netq_slot_t *slots;
    uint32_t capacity; /* number of slots, a power of two; one is always left empty */
    volatile uint32_t head; /* next slot to write (producer) */
    volatile uint32_t tail; /* next slot to read (consumer) */
} netq_t;

static inline void netq_init(netq_t *q, netq_slot_t *slots, uint32_t capacity)
{
    q->slots = slots;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
}

static inline uint32_t netq_count(const netq_t *q)
{
    uint32_t h = q->head, t = q->tail;
    return (h - t) & (q->capacity - 1);
}

static inline int netq_full(const netq_t *q)
{
    return ((q->head + 1) & (q->capacity - 1)) == q->tail;
}

static inline int netq_empty(const netq_t *q)
{
    return q->head == q->tail;
}

/* Producer: slot to fill, or NULL if full. Call netq_commit when done. */
static inline netq_slot_t *netq_reserve(netq_t *q)
{
    return netq_full(q) ? (netq_slot_t *)0 : &q->slots[q->head];
}

static inline void netq_commit(netq_t *q)
{
    q->head = (q->head + 1) & (q->capacity - 1);
}

/* Producer convenience: copy a whole frame in. Returns 0 if full or too long. */
static inline int netq_push(netq_t *q, const uint8_t *frame, uint32_t length)
{
    netq_slot_t *s;
    if (length > NETQ_FRAME_MAX)
    {
        return 0;
    }
    s = netq_reserve(q);
    if (!s)
    {
        return 0;
    }
    memcpy(s->data, frame, length);
    s->length = (uint16_t)length;
    netq_commit(q);
    return 1;
}

/* Consumer: oldest slot, or NULL if empty. Call netq_pop when done. */
static inline netq_slot_t *netq_peek(netq_t *q)
{
    return netq_empty(q) ? (netq_slot_t *)0 : &q->slots[q->tail];
}

static inline void netq_pop(netq_t *q)
{
    q->tail = (q->tail + 1) & (q->capacity - 1);
}

#endif
