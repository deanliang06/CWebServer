#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>



#include "types.h"
#include "helper.h"
#include "socket.h"
#include "workers.h"

#define INC_NUM_ARGS 1
#define WORKER_NUM_ERR 2
#define WORKER_TIMEOUT_ERR 4
#define INCORRECT_OPT 5
#define WORKER_LIFE_ERR 8
#define SOCKET_CREAT_ERR 9
#define SOCKET_BIND_ERR 10
#define SOCKET_LISTEN_ERR 11
#define IMPROPER_SIGNAL 12

#define debug_print 100

#define DAEMON 700
#define WORKER 701

#define HEALTH_FILE_NAME_BUF 128

Config cf = {};
Routes routes = {NULL};
Running running_stats = {};
int child_pid = -1;


void start_loop(Config *cf, Running *run_stats); //main loop function
void kill_all(Config *cf, Running *run_stats);

void init_config_defaults(Config *cf) {
    cf->timeout = 30;
    cf->max_lifetime = 120;
    cf->workers = 4;
    cf->routes = &routes;
}
void print_err(int err) {
    char *string;
    switch(err) {
        case INC_NUM_ARGS:
            string = "Provide 0 to 3 arguments: workers, timeout seconds, lifetime seconds";
            break;
        case WORKER_NUM_ERR:
            string = "Please enter a worker count from 1 to 50";
            break;
        case WORKER_TIMEOUT_ERR:
            string = "Please enter a timeout from 5 to 960 seconds";
            break;
        case SOCKET_CREAT_ERR:
            string= "Something went wrong with socket creation";
            break;
        case SOCKET_BIND_ERR:
            string = "Something went wrong with socket binding";
            break;
        case SOCKET_LISTEN_ERR:
            string = "Something went wrong with socket listening";
            break;
        case WORKER_LIFE_ERR:
            string="Please enter a lifetime in between 30 seconds and an hour in seconds";
            break;
        case IMPROPER_SIGNAL:
            string="Signal setup error";
            break;
        case debug_print:
            string = "Got here";
            break;
        default:
            string = "Unknown error occurred";
    }

    printf("%s\n", string);
}

void printHelp() {
    printf("You are using C web server with the following expected format:\n\
        ./server.exe [framework entry file] [optional args] \n\
        (include -n for num workers and -t for timeout time -l for worker max lifetime)\n");
}


void kill_all_signal(int signum) {
    kill_all(&cf, &running_stats);
    exit(1);
}

void kill_child_signal(int signum) {
    kill(child_pid, SIGINT);
    exit(1);
}


int signal_shutdown_setup(int parent) {
    struct sigaction old = {};
    struct sigaction new = {};

    memset(&new, 0, sizeof(sigaction));
    if (parent) new.sa_handler = kill_child_signal;
    else new.sa_handler = kill_all_signal;
    new.sa_flags = SA_NODEFER;

    if (sigaction(SIGINT, &new, &old)) return -1;
    return 0;
}

int register_route(char *target, char* (*f)(char *, char *)) {
    Route *new_route = (Route *)malloc(sizeof(Route));
    new_route->f = f;
    new_route->next = routes.head_route;
    new_route->target = target;
    routes.head_route = new_route;
}

int kill_server() {
    kill(child_pid, SIGINT);
    return 0;
}

int start_server(int count, ...) {
    if (count < 0 || count > 3) {
        print_err(INC_NUM_ARGS);
        return 1;
    }

    init_config_defaults(&cf);
    
    va_list args;
    va_start(args, count);
    
    // Optional positional arguments: workers, polling timeout, worker lifetime.
    for (int i = 0; i < count; i++) {
        int arg = va_arg(args, int);
        if (i == 0) {
            if (arg > 0 && arg <= 50) {
                cf.workers = arg;
            } else {
                print_err(WORKER_NUM_ERR);
                va_end(args);
                return 1;
            }
        } else if (i==1) {
            if (arg >= 5 && arg <= 960) {
                cf.timeout = arg;
            } else {
                print_err(WORKER_TIMEOUT_ERR);
                va_end(args);
                return 1;
            }
        } else {
            if (arg >= 30 && arg <= 3600) {
                cf.max_lifetime = arg;
            } else {
                print_err(WORKER_LIFE_ERR);
                va_end(args);
                return 1;
            }
        }
    }

    va_end(args);
    signal_shutdown_setup(0);
    //fork so that framework thread isn't blocked by master loop
    int child_id = fork();
    if (!child_id) {
        start_loop(&cf, &running_stats);
        exit(0);
    } else {
        child_pid = child_id;
        signal_shutdown_setup(1);
    }

    return 0;
}

void init_running(Running *rn) {
    rn->fds_opened = 0;
    rn->tail = NULL;
    rn->head = NULL;
    int listen_fd;
    if ((listen_fd = init_listen_socket()) < 0) {
        print_err(12+listen_fd);//for debugging
    }
    rn->listen_socket = listen_fd;
}

void close_worker(int health_fd, int signal_fd, int listen_fd) {
    close(health_fd);
    close(signal_fd);
    close(listen_fd);
}

void close_other_fds(Running *run_stats, int fd_to_keep) {
    Worker *head = run_stats->head;
    while (head) {
        if (head->running_fd != fd_to_keep) {
            close(head->running_fd);
            close(head->signal_fd);
        }
        head = head->next;
    }
}

void kill_process_and_rm_list(Worker *proc, Running *run_stats) {
    //not necessary to be thread safe
    Worker *prev = proc->prev;
    Worker *next = proc->next;
    if (prev) {
        prev->next = next;
    } else {
        run_stats->head = next;
    }

    if (next) {
        next->prev = prev;
    } else {
        run_stats->tail = prev;
    }

    kill(proc->proc_id, SIGTERM);
    write(proc->signal_fd, "", 1);
    close(proc->running_fd);
    close(proc->signal_fd);
    free(proc);  
    return;
}

void kill_all(Config *cf, Running *run_stats) {
    Worker *head = run_stats->head;
    while (head != NULL) {
        kill_process_and_rm_list(head, run_stats);
        head = run_stats->head;
    }

    return;
}

int init_worker(Config *cf, Running *run_stats) {
    //create the health file for seeing health patterns
    char hfile_name[HEALTH_FILE_NAME_BUF];
    init_hfile(hfile_name, run_stats->fds_opened);
    FILE * f = tmpfile();
    int health_fd = fileno(f);

    //create UNIX socket for signal wakeups
    int pipes[2];
    if (pipe(pipes)) return -1;//if not

    //adding itself to the Running Linked list
    Worker *work = (Worker *)(malloc(sizeof(Worker)));
    if (!run_stats->tail) {
        run_stats->head = work;
    } else {
        run_stats->tail->next = work;
    }
    work->next = NULL;
    work->prev = run_stats->tail;
    work->running_fd = health_fd;
    work->signal_fd = pipes[1];

    //get rn's time
    struct timespec curLowest = {};
    timespec_get(&curLowest, TIME_UTC);
    work->start_time = curLowest.tv_sec;


    run_stats->tail = work;

    //forking and then clearing other fds that don't belong to it
    int new_proc_id = fork();
    if (new_proc_id) {
        work->proc_id = new_proc_id;
        close(pipes[0]);
    } else {
        close(pipes[1]);
        close_other_fds(run_stats, health_fd);
        run_worker_loop(cf, health_fd, pipes[0], run_stats->listen_socket);
        kill_process_and_rm_list(work, run_stats);
        return WORKER;
    }
    run_stats->fds_opened++;
    return 0;
}

void safe_remove_last_accessed(Running *run_stats) {
    Worker *head = run_stats->head;
    Worker *min = NULL;

    //get rn's time
    struct timespec curLowest = {};
    timespec_get(&curLowest, TIME_UTC);

    time_t minTime = curLowest.tv_sec;
    struct stat st = {};
    while (head != NULL) {
        //get the time of process in loop to find min
        int fd = head->running_fd;
        if (fstat(fd, &st)) {
            minTime = 0;
            min = head;
            break;
        }
        
        //if the health file has later access time prioritize killing it
        if (st.st_atime < minTime) {
            minTime = st.st_atime;
            min = head;
        }
    }

    kill_process_and_rm_list(head, run_stats);
}

int manage_worker_count(Config *cf, Running *run_stats) {
    Worker *head = run_stats->head;
    int count = 0;
    while (head != NULL) {
        head = head->next;
        count++;
    }

    int numNeeded =  cf->workers - count;
    if (numNeeded < 0) {
        //numNeeded to delete (we will delete last updated ones via custom sig_kill)
        for (int i = 0; i < -1*numNeeded; i++) {
            safe_remove_last_accessed(run_stats);
        }
    } else if (numNeeded > 0) {
        for (int i = 0; i < numNeeded; i++) {
            if (init_worker(cf, run_stats) == WORKER) exit(0);
        }
    }
    return 1;
}

int kill_old_workers(Config *cf, Running *run_stats) {
    Worker *head = run_stats->head;
    while (head) {
        struct timespec time = {};
        timespec_get(&time, TIME_UTC);

        Worker *next = head->next;
        if (time.tv_sec-cf->max_lifetime > head->start_time) kill_process_and_rm_list(head, run_stats);
        head = next;
    }
    return 1;
}

void start_loop(Config *cf, Running *run_stats) {
    init_running(run_stats);
    while(1) {
        kill_old_workers(cf, run_stats);
        manage_worker_count(cf, run_stats);
        sleep(1);
    }
}

