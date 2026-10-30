#include <time.h>

typedef struct Config {
    int workers;
    int max_lifetime;
    int timeout;
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