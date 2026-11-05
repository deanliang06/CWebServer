#include "types.h"

extern Routes routes;


void init_static_routes();
int register_route(char *target, char* (*f)(Headers *, char *), int wildcard);