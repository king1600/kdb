#ifndef K_BUFFER_H
#define K_BUFFER_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t pos;
    size_t len;
    size_t max;
    char *data;
} kbuf_t;

void kbuf_init(kbuf_t *buf);
void kbuf_free(kbuf_t *buf);
int kbuf_read(kbuf_t *buf, void *output, size_t size);
int kbuf_write(kbuf_t *buf, const char *data, const size_t size);

#endif // K_BUFFER_H