#include "io.h"
#include "buffer.h"

// ANSI-C libs
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

// iso-c/posix libs
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>

// linux kernel libs
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>

static inline uint64_t k_now(struct timeval *tv) {
    gettimeofday(tv, NULL);
    return (uint64_t)((tv->tv_sec * 1000) + (tv->tv_usec / 1000));
}

kio_ctx_t* kio_init(kio_ctx_t *ctx) {
    if (ctx == NULL) return NULL;
    ctx->data = NULL;
    ctx->epoll_fd = -1;
    ctx->clients = NULL;
    ctx->accept_cb = NULL;
    ctx->tasks = NULL;
    ctx->closed = 0;
    ctx->client_num = 0;
    return ctx;
}

void kio_close(kio_client_t *client) {
    if (client == NULL) return;

    // remove from epoll context
    epoll_ctl(client->ctx->epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);

    // close socket file descriptor
    close(client->fd);

    // call on_close callback
    if (client->on_close != NULL)
        ((kio_clientcb_t)client->on_close)(client);

    // free read buffer
    kbuf_free(&client->rbuf);

    // remove from context
    if (client->ctx != NULL) {
        kio_client_t *c = (kio_client_t*)client->ctx->clients;
        if (c != NULL && c->fd == client->fd) {
            client->ctx->clients = client->next;
        } else {
            while (c->next != NULL && ((kio_client_t*)c->next)->fd != client->fd)
                c = (kio_client_t*)c->next;
            if (c->next == client)
                c->next = client->next;
        }
        client->ctx->client_num--;
    }

    // free client
    free(client);
    client = NULL;
}

void kio_free(kio_ctx_t *ctx) {
    if (ctx == NULL || ctx->closed == 0) return;

    kio_task_t *task, *last_task;
    kio_client_t *client, *last_client;

    // free client objects
    client = (kio_client_t*)ctx->clients;
    while (client != NULL) {
        last_client = client;
        client = (kio_client_t*)client->next;
        if (last_client != NULL)
            kio_close(last_client);
    }
    ctx->clients = NULL;
    
    // free task objects
    task = ctx->tasks;
    while (task != NULL) {
        last_task = task;
        task = (kio_task_t*)task->next;
        if (last_task != NULL)
            free(last_task);
    }
    ctx->tasks = NULL;

    // set context officially closed
    ctx->closed = 1;
    ctx->client_num = 0;
}

int kio_call(kio_ctx_t *ctx, const uint64_t delay, void *data, kio_callback_t callback) {
    if (ctx == NULL) return -1;

    // create task obejct
    kio_task_t *task = malloc(sizeof(kio_task_t));
    if (task == NULL) {
        fprintf(stderr, "[KDB] Not enough memory for kio_task!\n");
        return -1;
    }
    task->data = data;
    task->next = NULL;
    task->callback = callback;

    // set expires as current time + ms delay
    struct timeval tv;
    task->expires = k_now(&tv) + delay;

    // append task to context
    if (ctx->tasks == NULL) {
        task->last = NULL;
        ctx->tasks = task;
    } else {
        kio_task_t *t = ctx->tasks;
        while (t->next != NULL)
            t = t->next;
        task->last = t;
        t->next = task;
    }

    // success creating task
    return 0;
}

void kio_write(kio_client_t *client, const char *data, const size_t len) {
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

static inline int kio_rtask(kio_client_t *client, const uint64_t goal_size, const char *str_goal, kio_clientcb_t callback) {
    // allocate memory for the task
    kio_rtask_t *task = malloc(sizeof(kio_rtask_t));
    if (task == NULL) {
        fprintf(stderr, "[KDB] Not enough memory for read task!\n");
        return -1;
    }
    
    // create the task based on parameters
    task->next = NULL;
    task->str_goal = str_goal;
    task->goal_size = goal_size;
    task->callback = callback;
    task->gtype = str_goal == NULL ? 0 : 1;

    // append to end of tasks for client
    if (client->rtasks == NULL) {
        client->rtasks = task;
    } else {
        kio_rtask_t *t = client->rtasks;
        while (t->next != NULL)
            t = t->next;
        t->next = task;
    }

    // return success
    return 0;
}

int kio_read_until(kio_client_t *client, const char *goal, kio_clientcb_t callback) {
    if (client == NULL) return -1;
    return kio_rtask(client, strlen(goal), goal, callback);
}

int kio_read(kio_client_t *client, const uint64_t goal, kio_clientcb_t callback) {
    if (client == NULL) return -1;
    return kio_rtask(client, goal, NULL, callback);
}

static inline int kio_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    flags |= O_NONBLOCK;
    flags = fcntl(fd, F_SETFL, flags);
    return (flags != 0) ? -1 : 0;
}

static int kio_server(const uint16_t port) {
    int server;
    struct sockaddr_in addr;

    // create server socket
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == -1) {
        fprintf(stderr, "[KDB] Failed to create server socket\n");
        return -1;
    }

    // prepare address for server socket
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // enable REUSEADDR before binding to fix "Address already in use" error
    int enable = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        fprintf(stderr, "Failed to set SO_REUSEADDR\n");
        close(server);
        return -1;
    }

    // bind the server to the created address
    if (bind(server, (struct sockaddr*)&addr, sizeof(addr))) {
        fprintf(stderr, "[KDB] Failed to bind server to address\n");
        close(server);
        return -1;
    }

    // set listen queue for server
    if (listen(server, SOMAXCONN)) {
        fprintf(stderr, "[KDB] Failed to listen on server\n");
        close(server);
        return -1;
    }

    // set server to non blocking mode
    if (kio_non_blocking(server)) {
        fprintf(stderr, "[KDB] Failed to set server non blocking\n");
        close(server);
        return -1;
    }

    // return server file descriptor
    return server;
}

static void kio_accept(kio_ctx_t *ctx, int server, struct epoll_event *event) {
    int fd;
    kio_client_t *client;
    socklen_t client_len;

    // accept incoming clients
    while (1) {

        // create kio_client object for incoming fd
        client = malloc(sizeof(kio_client_t));
        if (client == NULL) {
            fprintf(stderr, "[KDB] Not enough memory for k_client! closing client\n");
            close(fd);
            continue;
        }

        client_len = sizeof(client->addr);
        fd = accept(server, (struct sockaddr*)&client->addr, &client_len);

        // client valid fd
        if (fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; // finished accepting all clients
            fprintf(stderr, "[KDB] Failed to accept incoming client\n");
            break;
        }

        // set client fd non blocking
        if (kio_non_blocking(fd)) {
            fprintf(stderr, "[KDB] Failed to set client non-blocking\n");
            close(fd);
            continue;
        }

        // prepare client
        client->fd = fd;
        client->ctx = ctx;
        client->data = NULL;
        client->next = NULL;
        client->rtasks = NULL;
        client->on_close = NULL;
        kbuf_init(&client->rbuf);

        // register client to epoll context
        event->data.fd = fd;
        event->data.ptr = client;
        event->events = EPOLLIN | EPOLLET;
        if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, fd, event) == -1) {
            fprintf(stderr, "[KDB] Failed to register client\n");
            kio_close(client);
            continue;
        }

        // add to context
        if (ctx->clients == NULL) {
            ctx->clients = client;
        } else {
            kio_client_t *c = (kio_client_t*)ctx->clients;
            while (c->next != NULL)
                c = c->next;
            c->next = client;
        }
        ctx->client_num++;

        // call the context client-accept callback
        if (ctx->accept_cb != NULL) {
            kio_clientcb_t callback = (kio_clientcb_t)ctx->accept_cb;
            callback(client);
        }
    }
}

static int kio_process_and_poll(kio_ctx_t *ctx, struct epoll_event *events) {
    if (ctx == NULL || ctx->closed) return -1;

    // prepare variables
    struct timeval tv;
    int wait_time = -1;
    uint64_t now, delay;
    kio_task_t *task, *next;

    // iterate through tasks
    task = ctx->tasks;
    while (task != NULL) {
        now = k_now(&tv);

        // a task is ready, update the ones around it, call it and free it
        if (now >= task->expires) {
            next = (kio_task_t*)task->next;
            task->callback(task->data);
            if (task->last != NULL) // make previous point to next
                ((kio_task_t*)task->last)->next = next;
            else if (task == ctx->tasks) // make root point to next
                ctx->tasks = (kio_task_t*)ctx->tasks->next;
            if (next != NULL) // make next's last (this) point to null
                next->last = NULL;
            // free and continue
            free(task);
            task = NULL;
            task = next;

        // task wasnt ready, but check if it should take priority in waiting
        } else {
            delay = task->expires - now;
            if (wait_time < 0 || delay < wait_time)
                wait_time = delay;
            task = (kio_task_t*)task->next;
        }
    }

    // poll for socket events given time to wait for tasks
    return epoll_wait(ctx->epoll_fd, events, KIO_MAX_EVENTS, wait_time);
}

static inline int kio_is_err(int e) {
    return ((e & EPOLLERR) || (e & EPOLLHUP) || (!(e & EPOLLIN))) ? 1 : 0;
}

int kio_run(kio_ctx_t *ctx, const uint16_t port) {
    // create runtime variables
    kbuf_t *buf;
    ssize_t n_read;
    kio_rtask_t *task;
    kio_client_t *client;
    struct epoll_event event;
    char read_buf[KIO_READ_SIZE];
    int polled, running, server, should_close;
    struct epoll_event events[KIO_MAX_EVENTS];

    // invalid context
    if (ctx == NULL || ctx->closed)
        return -1;

    // create epoll context for ctx
    ctx->epoll_fd = epoll_create1(0);
    if (ctx->epoll_fd == -1) {
        fprintf(stderr, "[KDB] Failed to create epoll context\n");
        return -1;
    }

    // create server
    server = kio_server(port);
    if (server < 0)
        return -1;

    // register server to epoll instance
    event.data.fd = server;
    event.events  = EPOLLIN | EPOLLET;
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, server, &event) == -1) {
        fprintf(stderr, "[KDB] Failed to register server on epoll instance\n");
        close(server);
        return -1;
    }

    // start io server
    running = 1;
    printf("[KDB] Server started on %d\n", port);
    while (!ctx->closed && running) {
        polled = kio_process_and_poll(ctx, events);
        if (polled < 0) break;
        while (polled-- && running) {

            // server event (either error or accept)
            if (events[polled].data.fd == server) {
                if (kio_is_err(events[polled].events))
                    running = 0;
                else
                    kio_accept(ctx, server, &event);

            // client event (either error or read event)
            } else if (events[polled].events & EPOLLIN) {
                client = (kio_client_t*)events[polled].data.ptr;
                buf = &client->rbuf;

                // handle client error
                if (client == NULL || kio_is_err(events[polled].events)) {
                    kio_close(client);
                    continue;
                }

                // process read task
                should_close = 0;
                while (!should_close) {
                    n_read = read(client->fd, read_buf, KIO_READ_SIZE);
                    if (n_read < 0) {
                        if (errno != EAGAIN)
                            should_close = 1;
                        break;
                    } else if (n_read == 0) {
                        should_close = 1;
                        break;
                    } else {
                        kbuf_write(buf, read_buf, n_read);
                    }
                }

                // process client read tasks
                while ((task = client->rtasks)) {
                    if (buf->len - buf->pos < task->goal_size)
                        break;
                    if (task->gtype)
                        if (!strstr(buf->data + buf->pos, task->str_goal))
                            break;
                    client->rtasks = task->next;
                    ((kio_clientcb_t)task->callback)(client);
                    free(task);
                    task = NULL;
                }

                // close client if should close
                if (should_close)
                    kio_close(client);
            }
        }
    }

    // free and return
    kio_free(ctx);
    return 0;
}
    