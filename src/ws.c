#include "ws.h"
#include "sha1.h"
#include <string.h>

static const int WS_GUID_SIZE = 36;
static const int WS_RESP_SIZE = 97
static const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char *WS_RESP = 
"HTTP/1.1 101 Switching Protocols\r\n"
"Upgrade: Websocket\r\n"
"Connection: Upgrade\r\n"
"Sec-WebSocket-Accept: ";

static int k_b64_mod[] = {0, 2, 1};
static const char* k_b64_table =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

// skey = char[28]
static const int kws_gen_key(const char *ckey, char *skey, const size_t csize) {
    char sha1_key[20];
    char guid_key[24 + WS_GUID_SIZE];
    uint32_t i, j, octet_a, octet_b, octet_c, triple;

    // invalid client key
    if (csize != 24)
        return -1;

    // SHA1(client_key + WS_GUID)
    memcpy(guid_key, ckey, 24);
    memcpy(guid_key + 24, WS_GUID, WS_GUID_SIZE);
    SHA1(sha1_key, guid_key, 24 + WS_GUID_SIZE);

    // base64 encode the sha1 result
    for (i = 0, j = 0; i < in_size;) {
        octet_a = i < in_size ? data[i++] : 0;
        octet_b = i < in_size ? data[i++] : 0;
        octet_c = i < in_size ? data[i++] : 0;
        triple  = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        skey[j++] = k_b64_table[(triple >> 3 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 2 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 1 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 0 * 6) & 0x3f];
    }
    for (i = 0; i < k_b64_mod[20 % 3]; i++)
        skey[27 - i] = '=';
}