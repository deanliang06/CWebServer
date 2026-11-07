#include <string.h>
#include <stdlib.h>

typedef struct Buffer {
    char *buf;
    int size;
    int capacity;
} Buffer;

void increase_buf(Buffer *buf) {
    int new_size = buf->capacity * 2;
    char *new_buf = (char *)malloc(sizeof(char)*new_size);
    memcpy(new_buf, buf->buf, buf->size);
    free(buf->buf);
    buf->buf = new_buf;
    buf->capacity = new_size;
}


void add_to_buf(Buffer *buf, char *char_to_add, int size) {
    while (buf->size+size >= buf->capacity) { //equals for null-terminating
        increase_buf(buf);
    }
    
    memcpy(buf->buf + buf->size, char_to_add, size);
    buf->size+=size;
}

Buffer *create_buf(int size) {
    char *c = (char *)malloc(sizeof(char) * size);
    Buffer *new_buf = (Buffer *)malloc(sizeof(Buffer));
    new_buf->buf = c;
    new_buf->capacity = size;
    new_buf->size = 0;
    return new_buf;
}

void add_null_term(Buffer *buf) {
    int first_empty = buf->size;
    buf->buf[first_empty] = '\0';
}

void free_dyna(Buffer *buf) {
    free(buf->buf);
    free(buf);
}