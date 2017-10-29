#include "ws.h"
#include "ws_parse.h"

void kws_parse_header(kio_client_t *client);
void kws_parse_size(kio_client_t *client);
void kws_parse_mask(kio_client_t *client);
void kws_parse_content(kio_client_t *client);
void kws_process_frame(kws_client_t *ws);

void kws_start_parsing(kws_client_t *ws) {
    kio_read(ws->client, 2, kws_parse_header);
}

void kws_parse_header(kio_client_t *client) {
    
}

void kws_parse_mask(kio_client_t *client) {
    
}

void kws_parse_size(kio_client_t *client) {
    
}

void kws_parse_content(kio_client_t *client) {
    
}

void kws_process_frame(kws_client_t *ws) {

}