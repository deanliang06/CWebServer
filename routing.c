#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "server.h"
#include "request.h"
#include "http_helper.h"
#include "helper.h"
#include "dyna_buffer.h"


Routes routes = {NULL};

char *resolve_root(char *target) {
    int target_len = strlen(target);

    char *root = "/";
    for (int i = 0; i < target_len+1; i++) {
        if (target[i] != root[i]) {
            return target;
        }
    }

    //if exits we know it is index.html
    char *index = (char *)malloc(sizeof(char) * 12);
    memcpy(index, "/index.html", 12);
    return index;
}

Response *serve_static(Headers *headers) {
    if (!check_get(headers->req_line->method)) return method_not_allowed();

    char *file_requested = headers->req_line->target;

    //only one rule is if they request '/' then it is resolved to index.html
    char *resolved_file = resolve_root(file_requested);

    char *requested_path = request_file_to_path(resolved_file);
    if (!requested_path) return NULL;
    
    Buffer *file_content = read_file(requested_path);

    if (!file_content) {
        free(requested_path);
        return internal_server_error();
    }
    return format_get_file(file_content, requested_path);
}

int register_route(char *target, Response* (*f)(Headers *, char *), int wildcard) {
    Route *new_route = (Route *)malloc(sizeof(Route));
    new_route->f = f;
    new_route->next = routes.head_route;
    new_route->target = target;
    new_route->wildcard = wildcard;
    routes.head_route = new_route;
    return 0;
}