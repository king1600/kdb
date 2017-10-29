#ifndef K_WS_H
#define K_WS_H

#include "io.h"

typedef struct {
    unsigned fin : 1;
    uint8_t opcode;
    char *data;
    size_t data_len;
} kws_frame_t;

typedef struct {
    void *data;
    void *on_pong;
    void *on_close;
    void *on_message;
    uint8_t state;
    kws_frame_t frame;
    kio_client_t *client;
} kws_client_t;


typedef void (*kws_callback_t)(kws_client_t*);
typedef void (kws_close_t)(kws_client_t*, uint16_t, const char*);

void kws_init(kio_ctx_t *ctx, kws_callback_t callback);

void kws_on_close(kws_client_t *client, kws_close_t callback);
void kws_on_pong(kws_client_t *client, kws_callback_t callback);
void kws_on_message(kws_client_t *client, kws_callback_t callback);

void kws_send(kws_client_t *client, const char *data);
void kws_ping(kws_client_t *client, const char *data, const size_t len);
void kws_close(kws_client_t *client, const uint16_t code, const char *reason);
void kws_send_bin(kws_client_t *client, const char* data, const size_t len);

#endif // K_WS_H