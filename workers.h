#ifndef CWEB_WORKERS_H
#define CWEB_WORKERS_H

#include "types.h"

void run_worker_loop(Config *cf, int health_fd, int unix_fd, int listen_fd);

#endif
