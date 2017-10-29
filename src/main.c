#include "kdb.h"
#include "ws.h"
#include <stdlib.h>
#include <signal.h>

static kio_ctx_t *kio_ctx;
#define KDB_SERVER_PORT 11011

void kdb_sig_cleanup(int sig) {
    kio_free(kio_ctx);
}

void kdb_client(kws_client_t *client) {
    printf("Accepted client\n");
}

int main(int argc, char **argv) {
    kio_ctx_t ctx;
    kio_ctx = &ctx;

    if (kio_init(kio_ctx) == NULL) {
        fprintf(stderr, "Failed to initalize kio_ctx\n");
        return EXIT_FAILURE;
    }

    struct sigaction act;
    act.sa_handler = kdb_sig_cleanup;
    sigaction(SIGINT, &act, NULL);

    kws_init(kio_ctx, kdb_client);

    if (kio_run(kio_ctx, KDB_SERVER_PORT))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
