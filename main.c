#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>


#include "types.h"
#include "helper.h"
#include "socket.h"
#include "workers.h"

#define WORKER_NUM_STRING 1
#define WORKER_NUM_ERR 2
#define WORKER_TIMEOUT_STRING 3
#define WORKER_TIMEOUT_ERR 4
#define INCORRECT_OPT 5
#define SOCKET_CREAT_ERR 6


#define DAEMON 700
#define WORKER 701

#define HEALTH_FILE_NAME_BUF 128

void start_loop(Config *cf, Running *run_stats); //main loop function

void init_config_defaults(Config *cf) {
    cf->timeout = 60;
    cf->workers = 4;
}
void print_err(int err) {
    char *string;
    switch(err) {
        case WORKER_NUM_ERR:
            string = "Please enter amount less than 50 and more than 0";
            break;
        case WORKER_NUM_STRING:
            string = "Please enter valid positive int for number of workers";
            break;
        case WORKER_TIMEOUT_ERR:
            string = "Please enter amount more than 5 and less than 960 seconds for timeout option";
            break;
        case WORKER_TIMEOUT_STRING:
            string = "Please valid positive int for timeout time";
            break;
        case SOCKET_CREAT_ERR:
            string= "Something went wrong with socket creation";
            break;
        default:
            string = "Unknown error occurred";
    }

    printf("%s\n", string);
}

void printHelp() {
    printf("You are using C web server with the following expected format:\n\
        ./server.exe [framework entry file] [optional args] \n\
        (include -n for num workers and -t for timeout time)\n");
}

int main(int argc, char *argv[]) {
    if (argc > 5) {
        fprintf(stderr, "Bro thinks there are more than 3 args");
        return 1;
    }

    Config cf = {};
    Running running_stats = {};
    init_config_defaults(&cf);
    
    int option;
    while ((option = getopt(argc, argv + 1, "n:t:h")) != -1) {
        switch(option) {
            case 'n':
                int workers = int_from_string(optarg);
                if (workers == -1) print_err(WORKER_NUM_STRING);
                else if (workers < 1 || workers > 50) print_err(WORKER_NUM_ERR);
                
                if (workers <= 0) return 1;
                cf.workers = workers;
                break;
            case 't':
                int timeout = int_from_string(optarg);
                if (timeout == -1) print_err(WORKER_TIMEOUT_STRING);
                else if (timeout < 5 || timeout > 960) print_err(WORKER_TIMEOUT_ERR);

                if (timeout < 5 || timeout > 960) return 1;
                cf.timeout = timeout;
                break;
            case 'h':
                printHelp();
                return 0;
            default:
                print_err(INCORRECT_OPT);
                return 1;
        }
    }

    start_loop(&cf, &running_stats);
    return 0;
}

void init_running(Running *rn) {
    rn->fds_opened = 0;
    rn->tail = NULL;
    rn->head = NULL;
    int listen_fd;
    if ((listen_fd = init_listen_socket()) == -1) {
        print_err(SOCKET_CREAT_ERR);
    }
    rn->listen_socket = listen_fd;
}

void init_hfile(char *buf, int num_name) {
    int idx = 0;
    while (num_name > 0) {
        int digit = num_name % 10;
        num_name/=10;
        buf[idx] = '0'+digit;
        idx++;
    }
    memcpy(buf+idx, "health", 7);
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

void kill_all(Config *cf) {
    return;
}

void kill_process_and_rm_list(Worker *proc) {
    return;
}

int manage_workers(Config *cf, Running *run_stats) {
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
                    kill_all(cf);
                    return 1;
                }
                
                //if the health file has later access time prioritize killing it
                if (st.st_atime < minTime) {
                    minTime = st.st_atime;
                    min = head;
                }
            }

            kill_process_and_rm_list(head);
        }
    } else if (numNeeded > 0) {
        for (int i = 0; i < numNeeded; i++) {

            //create the health file for seeing health patterns
            char hfile_name[HEALTH_FILE_NAME_BUF];
            init_hfile(hfile_name, run_stats->fds_opened);
            int health_fd = open(hfile_name, O_RDONLY | O_EXCL | O_CREAT);

            //create UNIX socket for signal wakeups
            int unix_fd = socket(AF_UNIX, SOCK_DGRAM, 0);

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
            work->signal_fd = unix_fd;
            run_stats->tail = work;

            //forking and then clearing other fds that don't belong to it
            int new_proc_id = fork();
            if (new_proc_id) {
                work->proc_id = new_proc_id;
            } else {
                close_other_fds(run_stats, health_fd);
                run_worker_loop(cf->timeout, health_fd, unix_fd, run_stats->listen_socket);
                return WORKER;
            }
            run_stats->fds_opened++;
        }
    }
    return DAEMON;
}


void start_loop(Config *cf, Running *run_stats) {
    init_running(run_stats);
    while(1) {
        if (manage_workers(cf, run_stats) != DAEMON) return;
        sleep(0.5);
    }
}

