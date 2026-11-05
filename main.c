#include "server.h"
#include "http_helper.h"
#include "routing.h"
#include "unistd.h"

char *get_home(Headers *headers, char *req_body) {
    char *body =
        "<!doctype html>"
        "<html lang=\"en\">"
        "<head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>C Web Server</title>"
        "<style>"
        "body{font-family:system-ui,sans-serif;max-width:680px;"
        "margin:80px auto;padding:0 24px;background:#f5f7fb;color:#172033}"
        "main{background:white;padding:32px;border-radius:12px}"
        "a{color:#245dcc}"
        "</style>"
        "</head>"
        "<body><main>"
        "<h1>Hello from C!</h1>"
        "<p>Your web server is running.</p>"
        "<p><a href=\"/health\">Check server health</a></p>"
        "</main></body></html>";

    return format_get(body);
}


char *get_health(Headers *headers, char *req_body) {
    char *body = "{\"health\": \"ok\"}";
    return format_get(body);
}

int main() {
    register_route("/", get_home);
    register_route("/health", get_health);
    start_server(0);
}   
