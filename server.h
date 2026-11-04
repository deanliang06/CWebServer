#ifndef CWEB_SERVER_H
#define CWEB_SERVER_H

#include "request.h"

int start_server(int count, ...);
int start_server_no_head(int count, ...);
void kill_server(void);
int register_route(char *target, char* (*f)(Headers *, char *));

#endif
