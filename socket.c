#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>


int init_listen_socket() {
    struct sockaddr_in adr = {};
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = INADDR_ANY;
    adr.sin_port = htons(8000);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(sfd, (struct sockaddr *)&adr, sizeof(adr))) {
        return -1;
    }

    if (listen(sfd, 10)) {
        return -1;
    }

    return sfd;
}

int accept_conn(int listen_fd) {
    int new_fd = accept(listen_fd, NULL, NULL);

    if (new_fd == -1) return -1; //some err handle necessary
    return new_fd;
}