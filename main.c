#include "server.h"
#include "http_helper.h"
#include "routing.h"


char *get_health(Headers *headers, char *req_body) {
    char *body = "{\"health\": \"ok\"}";
    return format_get(body);
}

int main() {
    register_route("/health", get_health, 0);
    start_server(0);
}   
