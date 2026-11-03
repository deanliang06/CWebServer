#include "server.h"
#include "unistd.h"


char *get_health(char *method, char *body) {
    return "HTTP/1.1 200 OK\r\n"
    "Content-Length: 15\r\n\r\n"
    "{\"health\":\"ok\"}\r\n\r\n";
}
int main(int argc, char *argv[]) {
    register_route("/health", get_health);
    start_server(0);
    sleep(50);
    kill_server();
}