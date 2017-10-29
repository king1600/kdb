#ifndef K_IO_H
#define K_IO_H

#include "buffer.h"
#include <stdio.h>
#include <netinet/in.h>

#ifndef KIO_READ_SIZE
#define KIO_READ_SIZE 4096
#endif

#ifndef KIO_MAX_EVENTS
#define KIO_MAX_EVENTS 128
#endif

typedef void (*kio_callback_t)(void *data);

typedef struct {
    void *data;
    void *next;
    void *last;
    uint64_t expires;
    kio_callback_t callback;
} kio_task_t;

typedef struct {
    void *next;
    void* callback;
    const char *str_goal;
    unsigned gtype : 1;
    uint64_t goal_size;
} kio_rtask_t;

typedef struct {
    void *data;
    int epoll_fd;
    void *clients;
    void *accept_cb;
    unsigned closed : 1;
    unsigned running : 1;
    kio_task_t *tasks;
    uint64_t client_num;
} kio_ctx_t;

typedef struct {
    int fd;
    void *data;
    void *next;
    void *on_close;
    kbuf_t rbuf;
    kio_ctx_t *ctx; 
    kio_rtask_t *rtasks;
    struct sockaddr_in addr;
} kio_client_t;

typedef void (*kio_clientcb_t)(kio_client_t *client);

kio_ctx_t* kio_init(kio_ctx_t *ctx);
void kio_free(kio_ctx_t *ctx);
void kio_close(kio_client_t *client);
int kio_run(kio_ctx_t *ctx, const uint16_t port);
int kio_call(kio_ctx_t *ctx, const uint64_t delay, void *data, kio_callback_t callback);
void kio_write(kio_client_t *client, const char *data, const size_t len);
int kio_read(kio_client_t *client, const uint64_t goal, kio_clientcb_t callback);
int kio_read_until(kio_client_t *client, const char *goal, kio_clientcb_t callback);

#endif // K_IO_H