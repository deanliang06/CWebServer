#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>


int init_listen_socket() {
    struct sockaddr_in adr = {};
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = INADDR_ANY;
    adr.sin_port = htons(8000);

    errno = 0;
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    //After all fds are closed the kernel still holds onto connection 
    int reuse = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));


    if (errno == EACCES) {
        return -3;
    }

    if (bind(sfd, (struct sockaddr *)&adr, sizeof(adr))) {
        perror("bind");
        return -2;
    }


    if (listen(sfd, 10)) {
        return -1;
    }

    return sfd;
}

int accept_conn(int listen_fd) {

    errno = 0;
    int new_fd = accept(listen_fd, NULL, NULL);
    if (errno == EAGAIN || errno == EWOULDBLOCK) return -1; //if nothing to accept (other worker handled)

    return new_fd;
}