int start_server(int count, ...);
int kill_server();
int register_route(char *target, char* (*f)(char *, char *));