#include "db.h"
#include "ws.h"

#include <stdlib.h>
#include <signal.h>

static kio_ctx_t *io_ctx;

static void kdb_sig_cleanup(int sig) {
    kio_free(io_ctx);
}

static void kdb_on_message(kws_client_t *client, const char *data, size_t len) {
    printf("Got message: %.*s\n", (int)len, data);
}

static void kdb_on_close(kws_client_t *client, uint16_t code, const char *reason, size_t len) {
    printf("Closing with code: %hu and reason: %.*s\n", code, (int)len, reason);
}

static void kdb_on_client(kws_client_t *client) {
    printf("Accepted client\n");
    kws_on_close(client, kdb_on_close);
    kws_on_message(client, kdb_on_message);
}

int kdb_run(kio_ctx_t *ctx, const uint16_t port) {
    io_ctx = ctx;

    // bind CTRL-C to exit
    struct sigaction sig_handler;
    sig_handler.sa_handler = kdb_sig_cleanup;
    sigaction(SIGINT, &sig_handler, NULL);

    // start websocket server
    kws_init(io_ctx, kdb_on_client);
    return kio_run(io_ctx, port);
}