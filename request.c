#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helper.h"
#include "types.h"
#include "request.h"

#define PROTOCOL_NOT_SUPPORTED ""

typedef enum {
    METHOD,
    TARGET,
    PROTOCOL
} RequestIdx;


typedef enum {
    HOST = 3,
    CONTENT_TYPE = 4,
    CONTENT_LENGTH = 5,
    TRANSFER_ENC = 6,
    CONN = 7,
    ACCEPT = 8,
    AUTH = 9
} HeaderType;

void parse_req_line(char *line, int size, RequestLine *rq) {
    RequestIdx type = METHOD;

    int prevBase = 0;
    for (int i = 0; i < size; i++) {
        if (line[i] == ' ' || i == size - 1) {
            char *extracted = (char *)malloc(sizeof(char) * i-prevBase+1);
            memcpy(extracted, line+prevBase, i-prevBase);
            extracted[i-prevBase] = '\0';

            if (type == METHOD) {
                rq->method = extracted;
            } else if (type == TARGET) {
                rq->target = extracted;
            } else {
                rq->protocol = extracted;
            }

            type++;
            prevBase = i+1;
        }
    }
}

int end_of_header(char *buf, int buf_size){
    for (int i = 0; i < buf_size - 3; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') return 1;
    }
    return 0;
}

int body_enough(char *buf, int buf_size, int body_len)  {
    for (int i = 0; i < buf_size - 3; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n' && buf_size > i+3+body_len) return 1;
    }
    return 0;
}

char *extract_body(char *buf, int buf_len, int body_len) {

    for (int i = 0; i < buf_len - 3; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            char *body = (char *)malloc(sizeof(char) * (body_len + 1));
            memcpy(body, buf+i+4, body_len);
            body[body_len] = '\0';
            return body;
        }
    }
    return NULL;
}

void parse_header(Headers *req_config, char *buf, int len) {
    char *supported_headers[7] = {"Host", "Content-Type", "Content-Length", "Transfer-Encoding", "Connection", "Accept", "Authorization"};

    int end_header_key = -1;
    int key_found = 0;

    char *header_key;
    for (int i = 0; i < len; i++) {
        if (buf[i] == ':') {
            header_key = (char *)malloc(sizeof(char) * i+1);
            memcpy(header_key, buf, i);
            header_key[i] = '\0';

            end_header_key = i;
            key_found = 1;
            break;
        }
    }   

    if (!key_found) return;

    for (int i = 0; i < 7; i++) {
        if (!strcasecmp(header_key, supported_headers[i])) {

            //the : is at index end_header_key and there is a space after :
            //therfore len-end_header_key
            char *header_val = (char *)malloc(sizeof(char) * len-end_header_key-1);
            memcpy(header_val, buf+end_header_key+2, len-end_header_key-2);
            header_val[len-end_header_key-1] = '\0';

            if (i+3 == HOST) {
                req_config->host = header_val;
            } else if (i+3 == CONTENT_TYPE) {
                req_config->content_type = header_val;
            } else if (i+3 == CONTENT_LENGTH) {
                int content_len = int_from_string(header_val);
                req_config->content_length = content_len;
            } else if (i+3 == TRANSFER_ENC) {
                req_config->transfer_enc = header_val;
            } else if (i+3 == CONN) {
                req_config->conn = header_val;
            } else if (i+3 == ACCEPT)  {
                req_config->accept = header_val;
            } else if (i+3 == AUTH) {
                req_config->auth = header_val;
            }

            break;
        }
    }
}
Headers *parse_headers(char * full_req, int size) {
    Headers *req_config = (Headers *)malloc(sizeof(Headers));
    req_config->content_length = 0;

    RequestLine *req_line = (RequestLine *)malloc(sizeof(RequestLine));
    int request_line_size = 0;
    for (int i = 0; i < size-1; i++) {
        if (full_req[i] == '\r' && full_req[i+1] == '\n') {
            request_line_size = i+1;
            break;
        }
    }

    //pretty obvious
    parse_req_line(full_req, request_line_size, req_line);

    if (strcmp(req_line->protocol, "HTTP/1.1")) {
        return NULL;
    }


    int base = request_line_size+1;
    for (int i = request_line_size+1; i < size-2; i++) {
        if (full_req[i] == '\r' && full_req[i+1] == '\n') {
            parse_header(req_config, full_req+base, i-base);
            base = i+2;
        }
    }

    req_config->req_line = req_line;
    return req_config;
}

char *get_response(Config *cf, Headers *headers, char *body) {
    Route *head = cf->routes->head_route;
    while (head) {
        if (head->wildcard) {

        } else if (!strcmp(headers->req_line->target, head->target)) {
            char *resp = head->f(headers, body);
            return resp;
        }

        head = head->next;
    }
    return NULL;
}

char *target_not_found_response() {
    return
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 9\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Not Found";
}
