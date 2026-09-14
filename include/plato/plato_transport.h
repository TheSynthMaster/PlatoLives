#ifndef PLATO_TRANSPORT_H
#define PLATO_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#define CYBER1_DEFAULT_HOST "cyberserv.org"
#define CYBER1_DEFAULT_PORT 8005

typedef struct plato_transport plato_transport_t;

struct plato_transport {
    int socket_fd;
    bool connected;
    char host[128];
    int port;
    FILE *log_file;
    bool logging_enabled;
    pthread_mutex_t log_mutex;
    uint64_t rx_packet_no;
    uint64_t tx_packet_no;
    uint64_t rx_stream_offset;
    uint64_t tx_stream_offset;
    uint8_t rx_previous_raw;
    uint8_t tx_previous_raw;
    bool rx_previous_valid;
    bool tx_previous_valid;
    char last_error[128];
};

plato_transport_t* plato_transport_create(void);
void plato_transport_destroy(plato_transport_t *t);
bool plato_transport_connect(plato_transport_t *t, const char *host, int port);
void plato_transport_disconnect(plato_transport_t *t);
int plato_transport_send(plato_transport_t *t, const uint8_t *data, size_t len);
int plato_transport_recv(plato_transport_t *t, uint8_t *buf, size_t max_len);
void plato_transport_set_logging(plato_transport_t *t, bool enabled);
bool plato_transport_is_logging(const plato_transport_t *t);
void plato_transport_log_msg(plato_transport_t *t, const char *fmt, ...);

#endif /* PLATO_TRANSPORT_H */
