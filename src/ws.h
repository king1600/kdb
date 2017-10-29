#ifndef K_WS_H
#define K_WS_H

#include "buffer.h"

#define KWS_OP_CONT 0x00
#define KWS_OP_TEXT 0x01
#define KWS_OP_BIN 0x02
#define KWS_OP_FIN 0x08
#define KWS_OP_PING 0x09
#define KWS_OP_PONG 0x0a

typedef struct {
    size_t size;
    uint8_t *data;
    unsigned fin : 1;
} kws_frame_t;

#endif // K_WS_H