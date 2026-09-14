#include <stdarg.h>
#include "plato/plato_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

static const char* byte_name(uint8_t value) {
    switch (value & 0x7F) {
        case 0x00: return "NUL";
        case 0x01: return "SOH/STOP";
        case 0x02: return "STX";
        case 0x03: return "ETX";
        case 0x08: return "BS/ERASE";
        case 0x09: return "HT";
        case 0x0A: return "LF";
        case 0x0B: return "VT/HELP";
        case 0x0C: return "FF/LAB";
        case 0x0D: return "CR/NEXT";
        case 0x11: return "DC1/SHIFT-STOP";
        case 0x12: return "DC2/DATA";
        case 0x19: return "BLOCK";
        case 0x1B: return "ESC";
        case 0x1C: return "POINT";
        case 0x1D: return "LINE";
        case 0x1F: return "ALPHA";
        case 0x7F: return "DEL";
        default: return NULL;
    }
}

static void format_time(char *out, size_t out_size) {
    struct timeval tv;
    struct tm tm_info;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm_info);
    snprintf(out, out_size, "%02d:%02d:%02d.%03d", tm_info.tm_hour, tm_info.tm_min,
             tm_info.tm_sec, (int)(tv.tv_usec / 1000));
}

static void log_ascii(FILE *f, const uint8_t *data, size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        uint8_t value = data[start + i] & 0x7F;
        fputc((value >= 0x20 && value <= 0x7E) ? (char)value : '.', f);
    }
}

static void log_traffic(plato_transport_t *t, const char *direction, const uint8_t *data, size_t len) {
    if (!t || !t->log_file || !data || len == 0) return;

    bool rx = strcmp(direction, "RX") == 0;
    uint64_t *packet_no = rx ? &t->rx_packet_no : &t->tx_packet_no;
    uint64_t *stream_offset = rx ? &t->rx_stream_offset : &t->tx_stream_offset;
    uint8_t *previous_raw = rx ? &t->rx_previous_raw : &t->tx_previous_raw;
    bool *previous_valid = rx ? &t->rx_previous_valid : &t->tx_previous_valid;
    char timestamp[32];
    format_time(timestamp, sizeof(timestamp));

    pthread_mutex_lock(&t->log_mutex);
    (*packet_no)++;
    fprintf(t->log_file,
            "[%s] [%s] packet=%llu bytes=%zu stream=[0x%08llX..0x%08llX] prev=%s%02X\n",
            timestamp, direction, (unsigned long long)*packet_no, len,
            (unsigned long long)*stream_offset,
            (unsigned long long)(*stream_offset + len - 1),
            *previous_valid ? "0x" : "--", *previous_valid ? *previous_raw : 0);

    for (size_t base = 0; base < len; base += 16) {
        size_t count = len - base < 16 ? len - base : 16;
        fprintf(t->log_file, "  RAW  %08llX: ", (unsigned long long)(*stream_offset + base));
        for (size_t i = 0; i < 16; i++) {
            if (i < count) fprintf(t->log_file, "%02X ", data[base + i]);
            else fputs("   ", t->log_file);
            if (i == 7) fputc(' ', t->log_file);
        }
        fputs(" | ", t->log_file);
        log_ascii(t->log_file, data, base, count);
        fputc('\n', t->log_file);

        fprintf(t->log_file, "  7BIT %08llX: ", (unsigned long long)(*stream_offset + base));
        for (size_t i = 0; i < count; i++) {
            fprintf(t->log_file, "%02X ", data[base + i] & 0x7F);
            if (i == 7) fputc(' ', t->log_file);
        }
        fputc('\n', t->log_file);
    }

    fputs("  EVENTS:\n", t->log_file);
    for (size_t i = 0; i < len; i++) {
        uint8_t raw = data[i];
        uint8_t seven = raw & 0x7F;
        uint64_t offset = *stream_offset + i;
        bool duplicate_iac = *previous_valid && *previous_raw == 0xFF && raw == 0xFF;
        const char *name = byte_name(raw);

        if (raw == 0xFF || duplicate_iac || name || raw != seven) {
            fprintf(t->log_file, "    %08llX raw=%02X 7bit=%02X",
                    (unsigned long long)offset, raw, seven);
            if (duplicate_iac) fputs(" IAC-DUP(second FF)", t->log_file);
            else if (raw == 0xFF) fputs(" IAC/data-FF(first or standalone)", t->log_file);
            if (name) fprintf(t->log_file, " %s", name);
            if (raw != seven) fputs(" HIGH-BIT", t->log_file);
            fputc('\n', t->log_file);
        }

        if (duplicate_iac) {
            *previous_valid = false;
            *previous_raw = 0;
        } else {
            *previous_valid = true;
            *previous_raw = raw;
        }
    }

    *stream_offset += len;
    fputc('\n', t->log_file);
    fflush(t->log_file);
    pthread_mutex_unlock(&t->log_mutex);
}

plato_transport_t* plato_transport_create(void) {
    plato_transport_t *t = (plato_transport_t *)calloc(1, sizeof(plato_transport_t));
    if (!t) return NULL;

    t->socket_fd = -1;
    pthread_mutex_init(&t->log_mutex, NULL);
    t->log_file = NULL;
    t->logging_enabled = false;
    return t;
}

void plato_transport_set_logging(plato_transport_t *t, bool enabled) {
    if (!t) return;
    pthread_mutex_lock(&t->log_mutex);
    if (enabled && !t->logging_enabled) {
        t->log_file = fopen("PlatoLives.log", "w");
        if (t->log_file) {
            char timestamp[32];
            format_time(timestamp, sizeof(timestamp));
            t->rx_packet_no = t->tx_packet_no = 0;
            t->rx_stream_offset = t->tx_stream_offset = 0;
            t->rx_previous_raw = t->tx_previous_raw = 0;
            t->rx_previous_valid = t->tx_previous_valid = false;
            fprintf(t->log_file, "=======================================================\n");
            fprintf(t->log_file, " PLATOLIVES DIAGNOSTIC NETWORK LOG\n");
            fprintf(t->log_file, " started=%s pid=%d\n", timestamp, getpid());
            fprintf(t->log_file, " RAW offsets are transport-stream offsets before decoding.\n");
            fprintf(t->log_file, " IAC-DUP marks the second FF in a TELNET FF FF pair.\n");
            fprintf(t->log_file, "=======================================================\n\n");
            fflush(t->log_file);
            t->logging_enabled = true;
        }
    } else if (!enabled && t->logging_enabled) {
        if (t->log_file) {
            fprintf(t->log_file, "=== LOG DISABLED rx_packets=%llu rx_bytes=%llu tx_packets=%llu tx_bytes=%llu ===\n",
                    (unsigned long long)t->rx_packet_no, (unsigned long long)t->rx_stream_offset,
                    (unsigned long long)t->tx_packet_no, (unsigned long long)t->tx_stream_offset);
            fclose(t->log_file);
            t->log_file = NULL;
        }
        t->logging_enabled = false;
    }
    pthread_mutex_unlock(&t->log_mutex);
}

bool plato_transport_is_logging(const plato_transport_t *t) {
    return t && t->logging_enabled;
}

void plato_transport_destroy(plato_transport_t *t) {
    if (!t) return;
    plato_transport_disconnect(t);
    pthread_mutex_lock(&t->log_mutex);
    if (t->log_file) {
        fprintf(t->log_file, "=== LOG CLOSED rx_packets=%llu rx_bytes=%llu tx_packets=%llu tx_bytes=%llu ===\n",
                (unsigned long long)t->rx_packet_no, (unsigned long long)t->rx_stream_offset,
                (unsigned long long)t->tx_packet_no, (unsigned long long)t->tx_stream_offset);
        fclose(t->log_file);
        t->log_file = NULL;
    }
    pthread_mutex_unlock(&t->log_mutex);
    pthread_mutex_destroy(&t->log_mutex);
    free(t);
}

bool plato_transport_connect(plato_transport_t *t, const char *host, int port) {
    if (!t || !host) return false;
    plato_transport_disconnect(t);
    strncpy(t->host, host, sizeof(t->host) - 1);
    t->host[sizeof(t->host) - 1] = '\0';
    t->port = port;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    t->last_error[0] = '\0';
    int gai_err = getaddrinfo(host, port_str, &hints, &res);
    if (gai_err != 0) {
        snprintf(t->last_error, sizeof(t->last_error), "%s", gai_strerror(gai_err));
        return false;
    }

    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) continue;
        int nodelay = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        snprintf(t->last_error, sizeof(t->last_error), "%s", strerror(errno));
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd == -1) {
        if (t->last_error[0] == '\0') {
            snprintf(t->last_error, sizeof(t->last_error), "Connection refused");
        }
        return false;
    }

    t->socket_fd = fd;
    t->connected = true;
    return true;
}

void plato_transport_disconnect(plato_transport_t *t) {
    if (!t) return;
    if (t->socket_fd >= 0) {
        close(t->socket_fd);
        t->socket_fd = -1;
    }
    t->connected = false;
}

int plato_transport_send(plato_transport_t *t, const uint8_t *data, size_t len) {
    if (!t || !t->connected || t->socket_fd < 0 || !data || len == 0) return -1;
    int sent = (int)send(t->socket_fd, data, len, 0);
    if (sent > 0) log_traffic(t, "TX", data, (size_t)sent);
    return sent;
}

int plato_transport_recv(plato_transport_t *t, uint8_t *buf, size_t max_len) {
    if (!t || !t->connected || t->socket_fd < 0 || !buf || max_len == 0) return -1;
    int received = (int)recv(t->socket_fd, buf, max_len, 0);
    if (received > 0) log_traffic(t, "RX", buf, (size_t)received);
    return received;
}

void plato_transport_log_msg(plato_transport_t *t, const char *fmt, ...) {
    if (!t || !t->logging_enabled || !t->log_file) return;
    pthread_mutex_lock(&t->log_mutex);
    va_list args;
    va_start(args, fmt);
    vfprintf(t->log_file, fmt, args);
    va_end(args);
    fflush(t->log_file);
    pthread_mutex_unlock(&t->log_mutex);
}
