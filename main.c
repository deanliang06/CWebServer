#include "server.h"
#include "http_helper.h"
#include "unistd.h"


char *get_health(Headers *headers, char *req_body) {
    char *body = "{\"health\": \"ok\"}";
    char *response = format_get(body);
    return response;
}

int main() {
    register_route("/health", get_health);
    start_server(0);
    sleep(50);
    kill_server();
}
