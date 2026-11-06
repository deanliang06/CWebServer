#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "server.h"
#include "request.h"
#include "http_helper.h"
#include "helper.h"


Routes routes = {NULL};

#define FILE_NOT_FOUND -1
#define IS_DIR 0

int register_route(char *target, char* (*f)(Headers *, char *), int wildcard);

char *handle_index_route(Headers *headers, char *body) {
    if (!check_get(headers->req_line->method)) return method_not_allowed();
    if (!file_exists_and_content("public/index.html")) return internal_server_error();


    char *file_content = read_file("public/index.html");
    return format_get_file(file_content, "public/index.html");
}

char *handle_style_route(Headers *headers, char *body) {
    if (!check_get(headers->req_line->method)) return method_not_allowed();
    if (!file_exists_and_content("public/style.css")) return format_unsuccessful_get();

    
    char *file_content = read_file("public/style.css");
    return format_get_file(file_content, "public/style.css");
}

char *handle_folder_route(Headers *headers, char *body) {
    if (!check_get(headers->req_line->method)) return method_not_allowed();

    char *file_requested = headers->req_line->target;
    char *path_requested = request_file_to_path(file_requested);
    
    if (file_exists_and_content(path_requested) == FILE_NOT_FOUND) return format_unsuccessful_get();

    char *file_content = read_file(path_requested);
    return format_get_file(file_content, path_requested);
}

void init_static_routes() {
    register_route("/", handle_index_route, 0);
    register_route("/style.css", handle_style_route, 0);
    register_route("/images/", handle_folder_route, 1);
}

int register_route(char *target, char* (*f)(Headers *, char *), int wildcard) {
    Route *new_route = (Route *)malloc(sizeof(Route));
    new_route->f = f;
    new_route->next = routes.head_route;
    new_route->target = target;
    new_route->wildcard = wildcard;
    routes.head_route = new_route;
    return 0;
}