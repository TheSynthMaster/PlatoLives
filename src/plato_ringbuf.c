#include "plato/plato_ringbuf.h"
#include <string.h>

void plato_ringbuf_init(plato_ringbuf_t *rb) {
    if (!rb) return;
    rb->head = 0;
    rb->tail = 0;
    pthread_mutex_init(&rb->mutex, NULL);
}

void plato_ringbuf_destroy(plato_ringbuf_t *rb) {
    if (!rb) return;
    pthread_mutex_destroy(&rb->mutex);
}

size_t plato_ringbuf_available(plato_ringbuf_t *rb) {
    if (!rb) return 0;
    pthread_mutex_lock(&rb->mutex);
    size_t count = (rb->head >= rb->tail) ? (rb->head - rb->tail) : (PLATO_RINGBUF_SIZE - rb->tail + rb->head);
    pthread_mutex_unlock(&rb->mutex);
    return count;
}

size_t plato_ringbuf_write(plato_ringbuf_t *rb, const uint8_t *data, size_t len) {
    if (!rb || !data || len == 0) return 0;
    pthread_mutex_lock(&rb->mutex);

    size_t written = 0;
    for (size_t i = 0; i < len; i++) {
        size_t next_head = (rb->head + 1) % PLATO_RINGBUF_SIZE;
        if (next_head == rb->tail) {
            break; /* Buffer pieno */
        }
        rb->buffer[rb->head] = data[i];
        rb->head = next_head;
        written++;
    }

    pthread_mutex_unlock(&rb->mutex);
    return written;
}

size_t plato_ringbuf_read(plato_ringbuf_t *rb, uint8_t *data, size_t max_len) {
    if (!rb || !data || max_len == 0) return 0;
    pthread_mutex_lock(&rb->mutex);

    size_t read_bytes = 0;
    while (rb->tail != rb->head && read_bytes < max_len) {
        data[read_bytes++] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % PLATO_RINGBUF_SIZE;
    }

    pthread_mutex_unlock(&rb->mutex);
    return read_bytes;
}
