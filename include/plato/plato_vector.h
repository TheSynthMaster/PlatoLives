#ifndef PLATO_VECTOR_H
#define PLATO_VECTOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    VEC_EVENT_LINE  = 0,
    VEC_EVENT_POINT = 1,
    VEC_EVENT_ERASE = 2,
    VEC_EVENT_CHAR  = 3,
    VEC_EVENT_BLOCK = 4
} plato_vector_type_t;

typedef struct {
    uint64_t timestamp_us;
    float x0, y0;
    float x1, y1;
    plato_vector_type_t type;
    uint8_t ch;
    uint8_t charset;
    uint8_t scale;
    uint8_t mode;
} plato_vector_event_t;

/* Ring Buffer tipizzato lock-free da 8192 slot (Zero overhead mutex) */
#define MAX_VECTOR_EVENTS 8192

typedef struct {
    plato_vector_event_t events[MAX_VECTOR_EVENTS];
    volatile uint32_t head;
    volatile uint32_t tail;
} plato_vector_queue_t;

#endif /* PLATO_VECTOR_H */
