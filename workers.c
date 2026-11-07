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
#include <time.h>
#include <errno.h>

#include "socket.h"
#include "types.h"
#include "request.h"
#include "worker_queue.h"
#include "http_helper.h"
#include "dyna_buffer.h"

#define INIT_REQUEST_SIZE 1024
#define RESP_PART 1024
#define HEADER_MAX 16384
#define REQ_MAX 100000

#define CONNECT_ERR 10
#define PIPE_READ_DEBUG 11
#define SOCKET_READ 12
#define SOCKET_RESP 13

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
    new.sa_flags = 0;

    if (sigaction(SIGINT, &new, &old)) return -1;
    return 0;
}

void throw_error(int socket_fd) {
    errno = 0;
    Response *response = internal_server_error();
    send(socket_fd, response->buf, response->size, MSG_DONTWAIT | MSG_NOSIGNAL);
    
    free(response->buf);
    free(response);
    if (errno) {
        printf("There was an error even sending an internal error\n");
    }
    close(socket_fd);
}


void handle_conn(int listen_fd, Config *cf) {
    int conn_fd = accept_conn(listen_fd);
    if (conn_fd == -1) return;

    struct timespec time = {};
    clock_gettime(CLOCK_MONOTONIC, &time);
    time_t start_time = time.tv_sec;

    //HTTP1.1 eventually
    char reuse_buf[INIT_REQUEST_SIZE];
    Buffer *buf = create_buf(INIT_REQUEST_SIZE);

    while (!end_of_header(buf->buf, buf->size)) {
        struct pollfd poll_struct = {conn_fd, POLLIN, 0};

        int n = poll(&poll_struct, 1, 5000);

        clock_gettime(CLOCK_MONOTONIC, &time);
        if (n < 0 || time.tv_sec - start_time >= cf->timeout) {
            throw_error(conn_fd);
            free_dyna(buf);
            return;
        } else if (n == 0) {
            continue;
        }

        errno = 0;
        int buf_added = read(conn_fd, reuse_buf, INIT_REQUEST_SIZE);

        if (errno == EINTR) continue;

        if (buf_added < 0 || buf->size+buf_added > 16384) {
            throw_error(conn_fd);
            free_dyna(buf);
            return;
        } else if (buf_added == 0) {//represents EOF or whatever
            break;
        }

        add_to_buf(buf, reuse_buf, buf_added);
    }

    if (!end_of_header(buf->buf, buf->size)) {
        throw_error(conn_fd);
        free_dyna(buf);
        return;
    }

    Headers *header_vals = parse_headers(buf->buf, buf->size);

    if (!header_vals) {
        throw_error(conn_fd);
        free_dyna(buf);
        return;
    }

    errno = 0;
    //put the body into buf if the content_length requires parsing
    if (header_vals->content_length)  {
        //read rest of body if there is body
        while (!body_enough(buf->buf, buf->size, header_vals->content_length)) {

            struct pollfd poll_struct = {conn_fd, POLLIN, 0};

            int n = poll(&poll_struct, 1, 5000);
            printf("%d\n", n);

            clock_gettime(CLOCK_MONOTONIC, &time);
            if (n < 0 || time.tv_sec - start_time >= cf->timeout) {
                throw_error(conn_fd);
                free_dyna(buf);
                free_header(header_vals);
                return;
            } else if (n == 0) {
                continue;
            }


            int buf_added = read(conn_fd, reuse_buf, INIT_REQUEST_SIZE);

            if (errno == EINTR) continue;

            if (buf_added < 0 || buf->size+buf_added > REQ_MAX) {
                throw_error(conn_fd);
                free_header(header_vals);
                free_dyna(buf);
                return;
            } else if (buf_added == 0) {
                break;
            }
            
            add_to_buf(buf, reuse_buf, buf_added);
        }
    }

    if (!body_enough(buf->buf, buf->size, header_vals->content_length)) {
        throw_error(conn_fd);
        free_dyna(buf);
        free_header(header_vals);
        return;
    }

    //extract the body
    char *body = NULL;
    if (header_vals->content_length) {
        body = extract_body(buf->buf, buf->size,header_vals->content_length);
    }

    //get response given headers and body
    Response *response = get_response(cf, header_vals, body);

    //if the no route then not found output
    if (!response) response = format_unsuccessful_get();

    //send message in blocks so that it doesn't fail when too large
    int resp_len = response->size;
    int sent = 0;

    //enforce max time to send
    clock_gettime(CLOCK_MONOTONIC, &time);
    start_time = time.tv_sec;

    //poll struct
    struct pollfd poll_struct = {conn_fd, POLLOUT, 0};

    while (resp_len > 0) {
        errno = 0;

        int n = poll(&poll_struct, 1, 5000);

        clock_gettime(CLOCK_MONOTONIC, &time);
        if (n < 0 || time.tv_sec - start_time > cf->timeout) {
            throw_error(conn_fd);
            if (header_vals->content_length) free(body);
            free_header(header_vals);
            free(response->buf);
            free(response);
            free_dyna(buf);    
            return;
        }

        if (n == 0 || !(poll_struct.revents & POLLOUT)) continue;


        int sent_part = send(conn_fd, response->buf+sent, (RESP_PART < resp_len) ? RESP_PART : resp_len, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent_part < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            continue;
        } else if (sent_part < 0 && errno != 0) {
            break;
        }
        resp_len-=sent_part;
        sent+=sent_part;
    }

    //close the connection (lol)
    close(conn_fd);
        
    //free all the things allocated in this function
    if (header_vals->content_length) free(body);
    free_header(header_vals);
    free(response->buf);
    free(response);
    free_dyna(buf);    
}

void update_health(int health_fd) {
    futimens(health_fd, NULL);
}

void handle_queue() {
    while (!empty(&queue)) {
        int signal = pop_queue(&queue);
        if (signal == SIGINT) health = 0;
    }
}

void run_worker_loop(Config *cf, int health_fd, int pipe_fd, int listen_fd) {
    int timeout = cf->timeout;
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
        update_health(health_fd);       
        if (!num_fds_ready) continue;
        
        //check if wakeup (in this case process signals)
        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
            //process signals
            handle_queue();
        
            //clear buffer and reset revents
            char toRead = 0;
            read(pipe_fd, &toRead, 1);
            fds[1].revents = 0;
            if (!health || fds[1].revents & (POLLERR | POLLHUP)) break;
        }

        if (fds[0].revents & POLLIN) {
            //connect to recieving port
            handle_conn(listen_fd, cf);
            fds[0].revents = 0;
        }
    }
}