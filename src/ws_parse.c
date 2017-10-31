#include "ws.h"
#include "ws_parse.h"
#include <string.h>

void kws_parse_header(kio_client_t *client);
void kws_parse_size(kio_client_t *client);
void kws_parse_mask(kio_client_t *client);
void kws_parse_content(kio_client_t *client);
int kws_process_frame(kws_client_t *ws, kws_frame_t *frame);

void kws_start_parsing(kws_client_t *ws) {
    kio_read(ws->client, 2, kws_parse_header);
}

void kws_parse_header(kio_client_t *client) {
    kbuf_t *buf = &client->rbuf;
    kws_frame_t *frame = &((kws_client_t*)client->data)->frame;

    // parse frame header
    const char *data = buf->data + buf->pos;
    frame->fin      = (data[0] >> 7) & 1;
    frame->opcode   = data[1] & 0x0F;
    frame->masked   = (data[1] >> 7) & 1;
    frame->data_len = data[1] & (~0x80);
    kbuf_read(buf, buf->data, 2);

    // decide what to do based on header
    if (frame->data_len == 0x7f) // read 8 bytes to get actual payload size
        kio_read(client, 8, kws_parse_size);
    else if (frame->data_len == 0x7e) // read 2 bytes to get actual payload size
        kio_read(client, 2, kws_parse_size);
    else if (frame->masked) // read mask of packet
        kio_read(client, 4, kws_parse_mask);
    else // go straight to reading content
        kio_read(client, frame->data_len, kws_parse_content);
}

void kws_parse_size(kio_client_t *client) {
    kbuf_t *buf = &client->rbuf;
    kws_frame_t *frame = &((kws_client_t*)client->data)->frame;

    // read data into frame->data_len
    const uint8_t num_bytes = frame->data_len == 0x7f ? 8 : 2;
    frame->data_len = 0;
    memcpy(&frame->data_len, buf->data + buf->pos, num_bytes);
    kbuf_read(buf, buf->data, num_bytes);

    // read mask or go straight to data
    if (frame->masked)
        kio_read(client, 4, kws_parse_mask);
    else
        kio_read(client, frame->data_len, kws_parse_content);
}

void kws_parse_mask(kio_client_t *client) {
    kbuf_t *buf = &client->rbuf;
    kws_frame_t *frame = &((kws_client_t*)client->data)->frame;

    memcpy(frame->mask, buf->data + buf->pos, 4);
    // kbuf_read(buf, buf->data, 4) // do not read because could rewrite mask
    kio_read(client, frame->data_len, kws_parse_content);
}

static inline void kws_mask(const char *mask, char *data, size_t len) {
    while (len--, data[len] ^= mask[len % 4]);
}

void kws_parse_content(kio_client_t *client) {
    kbuf_t *buf = &client->rbuf;
    kws_client_t *ws = (kws_client_t*)client->data;
    kws_frame_t *frame = &ws->frame;

    // get data and unmask it if needed
    frame->data = buf->data + buf->pos;
    if (frame->masked && frame->mask != NULL)
        kws_mask(frame->mask, frame->data, frame->data_len);

    // process the frame and reset
    const int should_close = kws_process_frame(ws, frame);
    frame->data = NULL;
    frame->mask = NULL;
    kbuf_read(buf, buf->data, frame->data_len);

    // close connection or continue reading
    if (should_close)
        kio_close(client);
    else
        kws_start_parsing(ws);
}

int kws_process_frame(kws_client_t *ws, kws_frame_t *frame) {
    char *payload;
    size_t payload_len;

    // frame is fragmented
    if (!frame->fin || frame->opcode == KWS_OP_CONT) {
        kbuf_write(&ws->fragment, frame->data, frame->data_len);
        return 0;
    }

    // frame is not fragmented
    if (ws->fragment.len - ws->fragment.pos > 0) {
        kbuf_write(&ws->fragment, frame->data, frame->data_len);
        payload = ws->fragment.data + ws->fragment.pos;
        payload_len = ws->fragment.len - ws->fragment.pos;
        kbuf_read(&ws->fragment, payload, payload_len);
    } else {
        payload = frame->data;
        payload_len = frame->data_len;
    }

    // decide what to do with the trame
    switch (frame->opcode) {

        // pong back packet
        case KWS_OP_PING:
            kws_send_raw(ws, payload, payload_len, KWS_OP_PONG);
            break;

        // ping was responded to, do callback
        case KWS_OP_PONG:
            if (ws->on_pong != NULL)
                ((kws_data_t)ws->on_pong)(ws, payload, payload_len);
            break;
    
        // message data was received
        case KWS_OP_BIN:
        case KWS_OP_TEXT:
            if (ws->on_message != NULL)
                ((kws_data_t)ws->on_message)(ws, payload, payload_len);
            break;

        // close frame was received
        case KWS_OP_FIN: {
            // client requested close
            if (ws->state == KWS_STATE_OPEN)
                kws_send_raw(ws, payload, payload_len, KWS_OP_FIN);
            return 1;
            break;
        }
    }

    // normal frame process
    return 0;
}