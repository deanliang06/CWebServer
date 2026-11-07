#include "types.h"

extern Routes routes;


void init_static_routes();
int register_route(char *target, Response* (*f)(Headers *, char *), int wildcard);