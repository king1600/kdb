#include "ws.h"
#include "sha1.h"
#include "ws_parse.h"
#include "ops.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <arpa/inet.h>

// data sizes
#define WS_CLRF_SIZE 2
#define WS_ECLRF_SIZE 4
#define WS_GUID_SIZE 36
#define WS_RESP_SIZE 97
#define WS_SHA1_SIZE 20
#define WS_CKEY_SIZE 24
#define WS_SKEY_SIZE 28
#define WS_FINAL_RESP_SIZE WS_RESP_SIZE + WS_SKEY_SIZE + WS_ECLRF_SIZE

// data strings
static const char *WS_CLRF = "\r\n";
static const char *WS_ECLRF = "\r\n\r\n";
static const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char *WS_RESP = 
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: Websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: ";

// base64 variables
static int k_b64_mod[] = {0, 2, 1};
static const char *k_b64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline void kws_tolower(char *str, size_t len) {
    while (len--, str[len] = tolower(str[len]));
}

static const int kws_gen_key(const char *ckey, char *skey, const size_t csize) {
    char sha1_key[WS_SHA1_SIZE];
    char guid_key[WS_CKEY_SIZE + WS_GUID_SIZE];
    uint32_t i, j, octet_a, octet_b, octet_c, triple;

    // invalid client key
    if (csize != WS_CKEY_SIZE)
        return -1;

    // SHA1(client_key + WS_GUID)
    memcpy(guid_key, ckey, WS_CKEY_SIZE);
    memcpy(guid_key + WS_CKEY_SIZE, WS_GUID, WS_GUID_SIZE);
    SHA1(sha1_key, guid_key, WS_CKEY_SIZE + WS_GUID_SIZE);

    // base64 encode the sha1 result
    for (i = 0, j = 0; i < WS_SHA1_SIZE;) {
        octet_a = i < WS_SHA1_SIZE ? (unsigned char)sha1_key[i++] : 0;
        octet_b = i < WS_SHA1_SIZE ? (unsigned char)sha1_key[i++] : 0;
        octet_c = i < WS_SHA1_SIZE ? (unsigned char)sha1_key[i++] : 0;
        triple  = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        skey[j++] = k_b64_table[(triple >> 3 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 2 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 1 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 0 * 6) & 0x3f];
    }

    // add the '=' padding
    for (i = 0; i < k_b64_mod[WS_SHA1_SIZE % 3]; i++)
        skey[WS_SKEY_SIZE - 1 - i] = '=';

    return 0;
}

static void kws_client_close(kio_client_t *client) {
    // close websocket client
    kws_client_t *ws = (kws_client_t*)client->data;
    kbuf_free(&ws->fragment);
    free(ws);
    ws = NULL;
}

static void kws_handshake(kio_client_t *client) {
    size_t len, value_len, key_len;
    uint8_t status = 0, method = 1;
    char *data = client->rbuf.data + client->rbuf.pos;
    char *pos, *value, *key, skey[WS_SKEY_SIZE] = { 0 };
    kbuf_read(&client->rbuf, data, strstr(data, WS_ECLRF) + 4 - data);

    // start parsing http request
    while ((pos = strstr(data, WS_CLRF))) {
        len = pos - data;
        if (len < 1) break;

        // parse first line
        if (method) {
            kws_tolower(data, len);
            if (strncmp(data, "get", 3))
                goto invalid_request;
            method = 0;
        
        // parse headers
        } else {
            key = data;
            if (!(value = strchr(data, ' ')))
                goto invalid_request;
            value++;
            value_len = pos - value;
            key_len = value - 2 - key;
            kws_tolower(key, key_len);

            // perform function based on header
            if (!strncmp(key, "sec-websocket-key", 17) && kws_gen_key(value, skey, value_len))
                goto invalid_request;
            kws_tolower(value, value_len);
            if (!strncmp(key, "upgrade", 7) && !strncmp(value, "websocket", 9))
                status |= KWS_HANDSHAKE_UPGRADE;
            if (!strncmp(key, "connection", 10) && !strncmp(value, "upgrade", 7))
                status |= KWS_HANDSHAKE_CONNECTION;
        }

        // goto next line
        data = pos + WS_CLRF_SIZE;
    }

    // check if valid websocket request
    if (skey[0] == 0 || status != (KWS_HANDSHAKE_UPGRADE | KWS_HANDSHAKE_CONNECTION))
        goto invalid_request;

    // send the http response
    char resp[WS_FINAL_RESP_SIZE];
    memcpy(resp, WS_RESP, WS_RESP_SIZE);
    memcpy(resp + WS_RESP_SIZE, skey, WS_SKEY_SIZE);
    memcpy(resp + WS_RESP_SIZE + WS_SKEY_SIZE, WS_ECLRF, WS_ECLRF_SIZE);
    kio_write(client, resp, WS_FINAL_RESP_SIZE);

    // start parsing websocket frames
    kws_client_t *ws = (kws_client_t*)client->data;
    ws->state = KWS_STATE_OPEN;
    kws_start_parsing(ws);

    // call accept for new ws client
    kws_callback_t callback = (kws_callback_t)client->ctx->data;
    if (callback != NULL)
        callback(ws);

    // end of function
    return;
    invalid_request:
        kio_close(client);
}

static void kws_accept(kio_client_t *client) {
    // get context and create ws_client 
    kws_client_t *ws = malloc(sizeof(kws_client_t));
    if (ws == NULL) {
        fprintf(stderr, "Not enough memory for kws_client_t");
        kio_close(client);
        return;
    }

    // initialize ws_client
    ws->data = NULL;
    ws->on_pong = NULL;
    ws->client = client;
    ws->on_close = NULL;
    ws->on_message = NULL;
    kbuf_init(&ws->fragment);
    ws->state = KWS_STATE_HANDSHAKE;

    // bind to client and read handshake
    client->data = ws;
    client->on_close = kws_client_close;
    kio_read_until(client, WS_ECLRF, kws_handshake);
}

void kws_init(kio_ctx_t *ctx, kws_callback_t callback) {
    if (ctx == NULL) return;
    ctx->data = callback;
    ctx->accept_cb = kws_accept;
}

void kws_on_close(kws_client_t *ws, kws_close_t callback) {
    ws->on_close = callback;
}

void kws_on_pong(kws_client_t *ws, kws_data_t callback) {
    ws->on_pong = callback;
}

void kws_on_message(kws_client_t *ws, kws_data_t callback) {
    ws->on_message = callback;
}

void kws_send(kws_client_t *ws, const char *data) {
    if (ws->state == KWS_STATE_OPEN)
        kws_send_raw(ws, data, strlen(data), KWS_OP_TEXT);
}

void kws_send_bin(kws_client_t *ws, const char* data, const size_t len) {
    if (ws->state == KWS_STATE_OPEN)
        kws_send_raw(ws, data, len, KWS_OP_BIN);
}

void kws_ping(kws_client_t *ws, const char *data, const size_t len) {
    if (ws->state == KWS_STATE_OPEN)
        kws_send_raw(ws, data, len, KWS_OP_PING);
}

void kws_close(kws_client_t *ws, const uint16_t code, const char *reason) {

    // create close frame data
    const size_t len = strlen(reason);
    char *data = malloc((sizeof(char) * len) + k_short_s);
    if (data == NULL) {
        fprintf(stderr, "[KDB] Not enough memory for close frame!");
        return;
    }

    // write close frame data
    memcpy(data, &code, k_short_s);
    memcpy(data + k_short_s, reason, len);

    // send close frame
    kws_send_raw(ws, data, len + k_short_s, KWS_OP_FIN);
    free(data);
    data = NULL;

    // perform callback and close websocket
    if (ws->on_close != NULL)
        ((kws_close_t)ws->on_close)(ws, code, reason, len);
    kio_close(ws->client);
}

void kws_send_raw(kws_client_t *ws, const char *data, const size_t len, const uint8_t opcode) {
    size_t length;
    uint8_t padding;
    char *output;
    
    // calculate size padding
    if (len < 126)
        padding = 0;
    else if (len < 65536)
        padding = 2;
    else
        padding = 8;
    
    // create message data
    length = 2 + len + padding;
    output = malloc(sizeof(output) * length);
    if (output == NULL) {
        fprintf(stderr, "[KDB] Not enough memory for websocket frame");
        return;
    }

    // write headers
    output[0] = (1 << 7) | opcode;
    output[1] = padding == 0 ? len : (padding == 2 ? 0x7e : 0x7f);
    if (padding == 2) {
        uint16_t size = htons(len);
        memcpy(output + 2, &size, k_short_s);
    } else if (padding == 8) {
        uint64_t size =be64toh(len);
        memcpy(output + 2, &size, k_long_s);
    }

    // write data
    memcpy(output + 2 + padding, data, len);
    kio_write(ws->client, output, length);
    free(output);
    output = NULL;
}