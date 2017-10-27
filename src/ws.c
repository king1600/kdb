#include "ws.h"
#include "sha1.h"
#include <string.h>

// data sizes
#define WS_GUID_SIZE 36
#define WS_RESP_SIZE 97
#define WS_SHA1_SIZE 20
#define WS_CKEY_SIZE 24
#define WS_SKEY_SIZE 28
#define WS_FRSP_SIZE WS_RESP_SIZE + WS_SKEY_SIZE + 4

// data context
static const char *WS_GUID = 
"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char *WS_RESP = 
"HTTP/1.1 101 Switching Protocols\r\n"
"Upgrade: Websocket\r\n"
"Connection: Upgrade\r\n"
"Sec-WebSocket-Accept: ";

// base64 variables
static int k_b64_mod[] = {0, 2, 1};
static const char* k_b64_table =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

// skey = char[28]
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
        octet_a = i < WS_SHA1_SIZE ? sha1_key[i++] : 0;
        octet_b = i < WS_SHA1_SIZE ? sha1_key[i++] : 0;
        octet_c = i < WS_SHA1_SIZE ? sha1_key[i++] : 0;
        triple  = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        skey[j++] = k_b64_table[(triple >> 3 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 2 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 1 * 6) & 0x3f];
        skey[j++] = k_b64_table[(triple >> 0 * 6) & 0x3f];
    }
    for (i = 0; i < k_b64_mod[WS_SHA1_SIZE % 3]; i++)
        skey[WS_SKEY_SIZE - 1 - i] = '=';

    return 0;
}