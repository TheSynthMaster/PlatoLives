#ifndef PLATO_RINGBUF_H
#define PLATO_RINGBUF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

#define PLATO_RINGBUF_SIZE 65536 /* 64 KB */

typedef struct {
    uint8_t buffer[PLATO_RINGBUF_SIZE];
    size_t head;
    size_t tail;
    pthread_mutex_t mutex;
} plato_ringbuf_t;

void plato_ringbuf_init(plato_ringbuf_t *rb);
void plato_ringbuf_destroy(plato_ringbuf_t *rb);
size_t plato_ringbuf_write(plato_ringbuf_t *rb, const uint8_t *data, size_t len);
size_t plato_ringbuf_read(plato_ringbuf_t *rb, uint8_t *data, size_t max_len);
size_t plato_ringbuf_available(plato_ringbuf_t *rb);

#endif /* PLATO_RINGBUF_H */
