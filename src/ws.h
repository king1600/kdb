#ifndef K_WS_H
#define K_WS_H

#include "io.h"

// websocket opcodes
#define KWS_OP_CONT 0x00
#define KWS_OP_TEXT 0x01
#define KWS_OP_BIN 0x02
#define KWS_OP_FIN 0x08
#define KWS_OP_PING 0x09
#define KWS_OP_PONG 0x0a

// client states
#define KWS_STATE_HANDSHAKE 0
#define KWS_STATE_OPEN 1
#define KWS_STATE_CLOSED 2
#define KWS_HANDSHAKE_UPGRADE 1
#define KWS_HANDSHAKE_CONNECTION 2

typedef struct {
    char *data;
    char *mask;
    uint8_t opcode;
    size_t data_len;
    unsigned fin : 1;
    unsigned masked : 1;
} kws_frame_t;

typedef struct {
    void *data;
    void *on_pong;
    void *on_close;
    void *on_message;
    uint8_t state;
    kbuf_t fragment;
    kws_frame_t frame;
    kio_client_t *client;
} kws_client_t;


typedef void (*kws_callback_t)(kws_client_t*);
typedef void (*kws_data_t)(kws_client_t*, const char*, size_t);
typedef void (*kws_close_t)(kws_client_t*, uint16_t, const char*);

void kws_init(kio_ctx_t *ctx, kws_callback_t callback);

void kws_on_close(kws_client_t *client, kws_close_t callback);
void kws_on_pong(kws_client_t *client, kws_data_t callback);
void kws_on_message(kws_client_t *client, kws_data_t callback);

void kws_send(kws_client_t *client, const char *data);
void kws_ping(kws_client_t *client, const char *data, const size_t len);
void kws_close(kws_client_t *client, const uint16_t code, const char *reason);
void kws_send_bin(kws_client_t *client, const char* data, const size_t len);
void kws_send_raw(kws_client_t *client, const char *data, const size_t len, const uint8_t opcode);

#endif // K_WS_H