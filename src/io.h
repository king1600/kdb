#ifndef K_IO_H
#define K_IO_H

#include "buffer.h"

#ifndef KIO_READ_SIZE
#define KIO_READ_SIZE 4096
#endif

#ifndef KIO_MAX_EVENTS
#define KIO_MAX_EVENTS 128
#endif

typedef void (*kio_callback_t)(void *data);
typedef void (*kio_readcb_t)(void *client, kbuf_t *buf);

typedef struct {
    void *data;
    void *next;
    uint64_t expires;
    kio_callback_t callback;
} kio_task_t;

typedef struct {
    void *next;
    uint64_t goal;
    kio_readcb_t callback;
} kio_rtask_t;

typedef struct {
    int epoll_fd;
    void *accept_cb;
    kio_task_t *tasks;
} kio_ctx_t;

typedef struct {
    int fd;
    void *data;
    kbuf_t rbuf;
    kio_ctx_t *ctx;
    kio_rtask_t *rtasks;
} kio_client_t;

typedef void (*kio_clientcb_t)(kio_client_t *client);


kio_ctx_t* kio_init(kio_ctx_t *ctx);
void kio_close(kio_client_t *client);
void kio_run(kio_ctx_t *ctx, const uint16_t port);
void kio_call(kio_ctx_t *ctx, const uint64_t delay, void *data, kio_callback_t callback);
void kio_write(kio_client_t *client, const uint8_t *data, const size_t len);
void kio_read(kio_client_t *client, const uint64_t amount, kio_readcb_t callback);

#endif // K_IO_H