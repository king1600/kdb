#include "io.h"
#include "buffer.h"

// ANSI-C libs
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// iso-c/posix libs
#include <fcntl.h>
#include <unistd.h>

// linux kernel libs
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>

uint64_t k_now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
}

kio_ctx_t* kio_init(kio_ctx_t *ctx) {
    // create epoll context
    ctx->epoll_fd = epoll_create1(0);
    if (ctx->epoll_fd < 0) return NULL;

    // zero out rest of structure
    ctx->tasks = NULL;
    ctx->accept_cb = NULL;
    memset(ctx->events, 0, sizeof(ctx->events));

    // return initialized context
    return ctx;
}

void kio_close(kio_client_t *client) {
    if (client == NULL) return;

    // remove from epoll context
    epoll_ctl(client->ctx->epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);

    // close socket file descriptor
    close(client->fd);

    // free client
    free(client);
    client = NULL;
}

int kio_call(kio_ctx_t *ctx, const uint64_t delay, void *data, kio_callback_t callback) {
    if (ctx == NULL) return 1;

    // create task obejct
    kio_task_t *task = malloc(sizeof(kio_task_t));
    if (task == NULL) return 1;
    task->data = data;
    task->next = NULL;
    task->callback = callback;
    task->delay = k_now() + delay;

    // append task to context
    if (ctx->tasks = NULL) {
        ctx->tasks = task;
    } else {
        kio_task_t *t = ctx->tasks;
        while (t->next != NULL)
            t = t->next;
        t->next = task;
    }

    // success creating task
    return 0;
}

void kio_write(kio_client_t *client, const uint8_t *data, const size_t len) {
    if (client == NULL) return;
    ssize_t offset = 0, n_write;

    // start writing data
    while (offset < len) {
        n_write = write(client->fd, data + offset, len - offset);
        
        // error or done writing data
        if (n_write == -1) {
            if (errno != EAGAIN)
                kio_close(client);
            break;
        }

        // there is no more data to write
        if (n_write == 0)
            break;

        // start writing where left off
        offset += n_write;
    }
}

void kio_read(kio_client_t *client, const uint64_t amount, kio_readcb_t callback) {
    if (client == NULL) return;

    // create read task
    kio_rtask_t *task = malloc(sizeof(kio_rtask_t));
    task->next = NULL;
    task->goal = amount;
    task->callback = callback;
    
    // append to end of tasks for client
    if (client->rtasks == NULL) {
        client->rtasks = task;
    } else {
        kio_rtask_t *t = client->rtasks;
        while (t->next != NULL)
            t = t->next;
        t->next = task;
    }
}

static inline int kio_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    flags |= O_NONBLOCK;
    flags = fcntl(fd, F_SETFL, flags);
    return (flags != 0) ? -1 : 0;
}

static int kio_server(const uint16_t port) {
    int ret, server;
    struct addrinfo hints;
    struct addrinfo *result, *addr;

    // set address search hints
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;       // Ipv4
    hints.ai_flags = AI_PASSIVE;     // All interfaces
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    // get available addresses
    char sport[5];
    sprintf(sport, "%d", port);
    printf("Using port %s\n", sport);
    ret = getaddrinfo(NULL, sport, &hints, &result);
    if (ret != 0) {
        fprintf(stderr, "Failed to fetch available server addresses: %s\n", gai_strerror(ret));
        return -1;
    }

    // try to create a server on one of the found addresses
    for (addr = result; addr != NULL; addr = addr->ai_next) {
        // create server socket
        server = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (server == -1) continue;
        // bind the server to the address
        ret = bind(server, addr->ai_addr, addr->ai_addrlen);
        if (ret == 0) break; // success bind
        close(server);
    }

    // check if found a valid address fot the server
    if (addr == NULL) {
        fprintf(stderr, "Failed to bind server address\n");
        return -1;
    }

    // set listen queue for server
    if (listen(server, SOMAXCONN) != 0) {
        fprintf(stderr, "Failed to listen on server\n");
        close(server);
        return -1;
    }

    // set server to non blocking mode
    if (kio_non_blocking(server) != 0) {
        fprintf(stderr, "Failed to set server non blocking\n");
        close(server);
        return -1;
    }

    // return server file descriptor
    return server;
}

static void kio_process(kio_ctx_t *ctx, int *polled, int *wait_time, struct epoll_event *events) {
    // TODO: task processing + epoll_wait
}

void kio_run(kio_ctx_t *ctx, const uint16_t port) {
    if (ctx == NULL) return;

    // create runtime variables
    int i, wait_time, polled;
    struct epoll_event event;
    struct epoll_event events[KIO_MAX_EVENTS];


}
    