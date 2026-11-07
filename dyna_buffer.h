typedef struct Buffer {
    char *buf;
    int size;
    int capacity;
} Buffer;

void add_to_buf(Buffer *buf, char *char_to_add, int size);
Buffer *create_buf(int size);
void add_null_term(Buffer *buf);
void free_dyna(Buffer *buf);