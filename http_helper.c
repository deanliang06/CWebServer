#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "helper.h"

#define FILLER_BYTES 20
#define BUF_SIZE 128

char *format_get(char *body) {//users responsibility to null-terminate body
    char *start_template = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
    int len = strlen(body);
    int template_len = strlen(start_template);

    char content_len_buf[BUF_SIZE];
    int bytes = string_from_int(content_len_buf, len);

    char *buf = (char *)malloc(sizeof(char) *(FILLER_BYTES + bytes + len + template_len + 1));
    memcpy(buf, start_template, template_len);
    memcpy(buf+template_len, "Content-Length: ", 16);


    memcpy(buf+template_len+16, content_len_buf, bytes);
    memcpy(buf+template_len+16+bytes, "\r\n\r\n", 4);
    memcpy(buf+FILLER_BYTES+template_len+bytes, body, len);
    buf[FILLER_BYTES+template_len+bytes+len] = '\0';
    return buf;
}

char *format_unsuccessful_get() {
    return 
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 9\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Not Found";
}

char *method_not_allowed() {
    return 
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";
}

char *internal_server_error() {
    return 
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";
}

int check_get(char *method) {
    int method_len = strlen(method);

    int start_idx = 0;
    for (int i = 0; i < method_len; i++) {
        if (method[i] != ' ') break;
        start_idx++;
    }

    char *get_string = "GET";
    for (int i = 0; i < 3; i++) {
        if (get_string[i] != method[start_idx+i]) return 0;
    }

    int it = start_idx+3;
    while(method[it]) {
        if (method[it] != ' ') return 0;
    }
    return 1;
}

