#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "socket.h"
#include "request.h"
#include "worker_queue.h"

#define INIT_REQUEST_SIZE 1024

#define CONNECT_ERR 10
#define PIPE_READ_DEBUG 11
#define SOCKET_READ 12

SignalQueue queue = {};
int health = 1;

void handle_signal(int signum) {
    add_queue(&queue, signum);
}

int init_signals() {
    struct sigaction old = {};
    struct sigaction new = {};

    memset(&new, 0, sizeof(sigaction));
    new.sa_handler = handle_signal;
    new.sa_flags = SA_NODEFER;

    if (sigaction(SIGTERM, &new, &old)) return -1;
    return 0;
}

static void print_err(int err) {
    char *string;
    switch(err) {
        case CONNECT_ERR:
            string = "Please enter amount less than 50 and more than 0";
            break;
        case PIPE_READ_DEBUG:
            string = "We are going to pipe";
            break;
        case SOCKET_READ:
            string= "We are going to socket";
            break;
        default:
            string = "Unknown error occurred";
    }

    printf("%s\n", string);
}

void handle_conn(int listen_fd) {
    int conn_fd = accept_conn(listen_fd);

    char *buf = (char *)malloc(sizeof(char) * INIT_REQUEST_SIZE);
    int curBufSize = INIT_REQUEST_SIZE;

    //track how many bytes per packet and sum over to see if
    //need to incrase buffer size
    int bufAdded = 0;
    int bufUsed = 0;

    //HTTP1.1
    while (!end_of_request(buf, curBufSize)) {
        bufAdded = read(conn_fd, buf, curBufSize-bufUsed);
        //if we add the rest of the buf then allocate new buf with twice size
        if (bufAdded == curBufSize-bufUsed) {
            char *newBuf = (char *)malloc(sizeof(char) * curBufSize*2);
            memcpy(newBuf, buf, curBufSize);
            free(buf);
            buf = newBuf;
            curBufSize*=2;
        }
        bufUsed+=bufAdded;
    }

    
    send(conn_fd, "Recieved\r\n\r\n", 9, MSG_DONTWAIT);
    close(conn_fd);

    printf("%s\n",buf);
    free(buf);
}

void update_health(int health_fd) {
    futimens(health_fd, NULL);
}

void handle_queue() {
    while (!empty(&queue)) {
        int signal = pop_queue(&queue);
        if (signal & SIGTERM) health = 0;
    }
}


void run_worker_loop(int timeout, int health_fd, int pipe_fd, int listen_fd) {
    if (init_signals()) return;
    struct pollfd fds[2];

    //listen socket for connections
    struct pollfd listen = {listen_fd, POLLIN, 0};
    fds[0] = listen;

    //unix socket for singals
    struct pollfd wakeup = {pipe_fd, POLLIN, 0};
    fds[1] = wakeup;

    while (health) {
        //wait on 
        int num_fds_ready = poll(fds, 2, timeout*1000);
        if (!num_fds_ready) continue;
        
        //check if wakeup (in this case process signals)
        if (fds[1].revents & POLLIN) {
            //process signals
            handle_queue();

            print_err(PIPE_READ_DEBUG);
            //clear buffer and reset revents
            char toRead = 0;
            read(pipe_fd, &toRead, 1);
            fds[1].revents = 0;
        }

        if (fds[0].revents & POLLIN) {
            //connect to recieving port
            print_err(SOCKET_READ);

            handle_conn(listen_fd);
            fds[0].revents = 0;
        }

        update_health(health_fd);       
        sleep(1);
    }
}