#include "kdb.h"
#include "io.h"
#include <stdlib.h>
#include <signal.h>

static kio_ctx_t *kio_ctx;
#define KDB_SERVER_PORT 11011

void kdb_sig_cleanup(int sig) {
    kio_free(kio_ctx);
}

void test_callback(void *data) {
    printf("Callback: %s\n", (const char*)data);
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

    if (kio_call(kio_ctx, 1000, "Hello world", test_callback))
        printf("Failed creating task for hello world\n");
    if (kio_call(kio_ctx, 2000, "How goes thou?", test_callback))
        printf("Failed creating task for how goes thou\n");
    if (kio_call(kio_ctx, 4000, "Pretty good", test_callback))
        printf("Failed creating task for pretty good\n");

    if (kio_run(kio_ctx, KDB_SERVER_PORT))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

