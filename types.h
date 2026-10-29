typedef struct Config {
    int workers;
    int timeout;
} Config;

typedef struct Worker {
    int proc_id;
    int running_fd;
    int signal_fd;
    struct Worker *prev;
    struct Worker *next;
} Worker;

typedef struct Running {
    Worker *head;
    Worker *tail;
    int fds_opened;
    int listen_socket;
} Running;