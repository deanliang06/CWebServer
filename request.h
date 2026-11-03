typedef struct RequestLine {
    char *method;
    char *target;
    char *protocol;
} RequestLine;

typedef struct Headers {
    RequestLine *req_line;
    char *host;
    char *content_type;
    int content_length;
    char *transfer_enc;
    char *conn;
    char *accept;
    char *auth;
} Headers;

int end_of_header(char *buf, int buf_size);
int body_enough(char *buf, int buf_size, int body_len);
char *extract_body(char *buf, int buf_len, int body_len);
char *get_response(Config *cf, Headers *headers, char *body);
char *target_not_found_response();
char *handle_request(char * fulRequest, int size);
Headers *parse_headers(char * full_req, int size);