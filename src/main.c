#include "db.h"
#include <stdlib.h>

#define KDB_SERVER_PORT 11011

int main(int argc, char **argv) {
    kio_ctx_t ctx;

    if (kio_init(&ctx) == NULL) {
        fprintf(stderr, "Failed to initalize kio_ctx\n");
        return EXIT_FAILURE;
    }

    if (kdb_run(&ctx, KDB_SERVER_PORT))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
