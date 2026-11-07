#ifndef CWEB_ROUTING_H
#define CWEB_ROUTING_H

#include "request.h"

extern Routes routes;


void init_static_routes();
int register_route(char *target, Response* (*f)(Headers *, char *), int wildcard);
Response *serve_static(Headers *headers);

#endif
