#ifndef CWEB_TYPES_H
#define CWEB_TYPES_H

#include <time.h>

struct Headers;

typedef struct Config {
    int workers;
    int max_lifetime;
    int timeout;
    struct Routes *routes;
} Config;

typedef struct Worker {
    int proc_id;
    int running_fd;
    int signal_fd;
    time_t start_time;
    struct Worker *prev;
    struct Worker *next;
} Worker;

typedef struct Running {
    Worker *head;
    Worker *tail;
    int fds_opened;
    int listen_socket;
} Running;

typedef struct Route {
    char *target;
    char *(*f)(struct Headers *, char *);
    struct Route *next;
} Route;
typedef struct Routes {
    Route *head_route;
} Routes;

#endif
