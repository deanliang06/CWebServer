#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "helper.h"

#define CONTENT_LINE_BYTES 20
#define BUF_SIZE 128

char *mime_types[5] =  {".html", ".css", ".js", ".png", ".svg"};
enum file_type {
    html,
    css,
    js,
    png,
    svg,
} file_type;

char *format_get(char *body) {//users responsibility to null-terminate body
    char *start_template = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
    int len = strlen(body);
    int template_len = strlen(start_template);

    char content_len_buf[BUF_SIZE];
    int bytes = string_from_int(content_len_buf, len);

    char *buf = (char *)malloc(sizeof(char) *(CONTENT_LINE_BYTES + bytes + len + template_len + 1));
    memcpy(buf, start_template, template_len);
    memcpy(buf+template_len, "Content-Length: ", 16);


    memcpy(buf+template_len+16, content_len_buf, bytes);
    memcpy(buf+template_len+16+bytes, "\r\n\r\n", 4);
    memcpy(buf+CONTENT_LINE_BYTES+template_len+bytes, body, len);
    buf[CONTENT_LINE_BYTES+template_len+bytes+len] = '\0';
    return buf;
}

char *format_get_file(char *body, char *file_name) {//users responsibility to null-terminate body

    //get which mime type
    enum file_type f = -1;
    int file_name_len = strlen(file_name);

    for (int i = 0; i < 5; i++) {
        int mime_len = strlen(mime_types[i]);
        int same_ext = 1;

        for (int j = 1; j < strlen(mime_types[i]) - 1; j++) {
            if (file_name[file_name_len-j] != mime_types[i][mime_len - j]) {
                same_ext = 0;
                break;
            }
        }

        if (same_ext) {
            f = i;
            break;
        }
    }

    //start which establishing closing of connection
    char *start_template = "HTTP/1.1 200 OK\r\nConnection: close\r\n";
    int template_len = strlen(start_template);


    //second part which establishes content type buffer
    if (f == -1) return NULL;

    char *content_type_beg;
    if (f < 3) {
        content_type_beg = "Content-type: text/";
    } else {
        content_type_beg = "Content-type: image/";
    }

    char *rest_of_mime;
    if (f == html) {
        rest_of_mime = "html\r\n";
    } else if (f == css) {
        rest_of_mime = "css\r\n";
    } else if (f == js) {
        rest_of_mime = "javascript\r\n";
    } else if (f == png) {
        rest_of_mime = "png\r\n";
    } else if (f == svg) {
        rest_of_mime = "svg+xml\r\n";
    }

    int mime_line_len = strlen(content_type_beg) + strlen(rest_of_mime);


    //get the string and num bytes for the length of body
    int body_len = strlen(body);

    char content_len_buf[BUF_SIZE];
    int content_line_len = string_from_int(content_len_buf, body_len) + CONTENT_LINE_BYTES;


    //allocate buf for everything
    char *buf = (char *)malloc(sizeof(char) *(template_len + mime_line_len+content_line_len+body_len+1));

    //copy template into buf first
    memcpy(buf, start_template, template_len);

    //copy content type
    memcpy(buf+template_len, content_type_beg, strlen(content_type_beg));
    memcpy(buf+ template_len+ strlen(content_type_beg), rest_of_mime, strlen(rest_of_mime));

    //copy content len
    memcpy(buf+template_len+mime_line_len, "Content-Length: ", 16);
    memcpy(buf+template_len+mime_line_len+16, content_len_buf, strlen(content_len_buf));
    memcpy(buf+template_len+mime_line_len+content_line_len-4, "\r\n\r\n", 4);

    //copy body
    memcpy(buf+template_len+mime_line_len+content_line_len, body, body_len);
    buf[template_len + mime_line_len+content_line_len+body_len] = '\0';
    // printf("Returned resp %s\n", buf);
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

