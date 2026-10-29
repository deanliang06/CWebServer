#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include "socket.h"
#include "request.h"

#define INIT_REQUEST_SIZE 1024

#define CONNECT_ERR 10
#define debug_log 11

void init_signals() {
    return;
}

static void print_err(int err) {
    char *string;
    switch(err) {
        case CONNECT_ERR:
            string = "Please enter amount less than 50 and more than 0";
            break;
        case debug_log:
            string = "We reach here";
            break;
        default:
            string = "Unknown error occurred";
    }

    printf("%s\n", string);
}

void run_worker_loop(int timeout, int health_fd, int unix_fd, int listen_fd) {
    init_signals();
    struct pollfd fds[2];

    //listen socket for connections
    struct pollfd listen = {listen_fd, POLLIN, 0};
    fds[0] = listen;

    //unix socket for singals
    struct pollfd wakeup = {unix_fd, POLLIN, 0};
    fds[1] = wakeup;

    while (1) {
        //wait on 
        int num_fds_ready = poll(fds, 2, timeout*1000);
        if (!num_fds_ready) continue;
        printf("%d\n", num_fds_ready);
        //check if wakeup (in this case process signals)
        if (fds[1].revents & POLLIN) {
            //process signals
        }

        if (fds[0].revents & POLLIN) {
            //connect to recieving port
            
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
            fds[0].revents = 0;
        }
        sleep(0.5);
    }
}