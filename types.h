typedef struct Config {
    int workers;
    int timeout;
} Config;

typedef struct Worker {
    int proc_id;
    int running_fd;
    int signal_fd;
    Worker *prev;
    Worker *next;
} Worker;

typedef struct Running {
    Worker *worker_head;
    Worker *tail;
    int fds_opened;
    int listen_socket;
} Running;