#include "buffer.h"
#include <stdlib.h>
#include <string.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

void kbuf_init(kbuf_t *buf) {
    buf->pos = 0;
    buf->len = 0;
    buf->max = 0;
    buf->data = NULL;
}

void kbuf_free(kbuf_t *buf) {
    if (buf->data != NULL) {
        free(buf->data);
        buf->data = NULL;
    }
}

int kbuf_read(kbuf_t *buf, void *output, size_t size) {
    if (buf == NULL)
        return -1;
    if (size == 0 || buf->len == 0 || buf->pos >= buf->len)
        return 0;
    size = MIN(size, buf->len - buf->pos);
    memcpy(output, buf->data + buf->pos, size);
    buf->pos += size;
    return 0;
}

int kbuf_write(kbuf_t *buf, const char *data, const size_t size) {
    if (buf == NULL)
        return -1;
    if (size == 0)
        return 0;

    // first write, initialze buffer to size
    if (buf->data == NULL) {
        buf->len = size;
        buf->max = size;
        buf->data = calloc(buf->max, sizeof(buf->data));
        memcpy(buf->data, data, size);
        return 0;
    }
    
    // reposition contents in order to avoid wasting front bytes
    if (buf->pos != 0) {
        memmove(buf->data, buf->data + buf->pos, buf->len - buf->pos);
        buf->len -= buf->pos;
        buf->pos = 0;
    }

    // resize buffer data if too small + extra room to avoid constant resizing
    if (buf->len + size > buf->max) {
        buf->max = (buf->len + size) * 12 / 10;
        buf->data = realloc(buf->data, buf->max);
        memset(buf->data + buf->len, 0, buf->max - buf->len);
    }

    // write the data
    memcpy(buf->data + buf->len, data, size);
    buf->len += size;
    return 0;
}