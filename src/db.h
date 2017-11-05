#ifndef K_DB_H
#define K_DB_H

#include "io.h"

typedef struct {
    char *name;
    uint8_t type;
    uint8_t name_len;
    size_t maxsize;
} kdb_column_t;

typedef struct {
    char *name;
    uint8_t name_len;
    uint8_t n_cols;
    kdb_column_t *cols;
} kdb_t;

typedef struct {
    kdb_t *dbs;
    size_t db_len;
} kdb_ctx_t;

int kdb_run(kio_ctx_t *ctx, const uint16_t port);

#endif