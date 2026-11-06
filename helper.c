#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>  
#include <sys/stat.h>

#define BUFLEN 1024

int int_from_string(char *string) {
    int num = 0;
    int idx = 0;
    while (string[idx]) {
        int c = string[idx] - '0';
        if (c < 0 || c > 9) return -1;
        if ((INT_MAX-c)/10 < num) return -1;
        num=num*10+c;
        idx++;
    }
    return num;    
}

int string_from_int(char *buf, int num) {
    int idx = 0;
    int replacement = num;

    while(num != 0) {
        num/=10;
        idx++;
    }

    int num_needed = idx;
    buf[idx] = '\0';

    while(idx) {
        idx--;
        buf[idx] = '0' + (replacement%10);
        replacement/=10;
    }

    return num_needed;
}

char *read_file(char *filename) {
    errno = 0;

    int fd = open(filename, O_RDONLY);
    if (fd == -1 || errno != 0) return NULL;

    int cur_size = BUFLEN;
    char *buf = (char *)malloc(sizeof(char) * cur_size);

    int bytes_read = 0;
    int buf_filled = 0;
    while ((bytes_read = read(fd, buf, cur_size-buf_filled))) {
        buf_filled+=bytes_read;
        if (buf_filled == cur_size) {
            cur_size*=2;
            char *new_buf = (char *)malloc(sizeof(char) * cur_size);
            memcpy(new_buf, buf, cur_size/2);
            free(buf);
            buf = new_buf;
        }
    }

    close(fd);
    return buf;
}

int file_exists_and_content(char *file_path) {
    struct stat st = {};
    if (stat(file_path, &st)) {
        return -1;
    } else if (st.st_mode & S_IFDIR) {
        return 0;
    }
    return 1;
}

char *request_file_to_path(char *requested_file) {
    int req_len = strlen(requested_file);
    int bytes_for_public = 7;

    char *new_buf = (char *)malloc(sizeof(char)*(req_len+bytes_for_public));
    memcpy(new_buf, "public", 6);
    memcpy(new_buf+bytes_for_public-1, requested_file, req_len+1);
    
    return new_buf;
}

int matching_front(char *wildcard, char *requested_resource) {
    int wild_len = strlen(wildcard);
    int requested_len = strlen(requested_resource);

    if (requested_len < wild_len) return 0;

    for (int i = 0; i < wild_len; i++) {
        if (requested_resource[i] != wildcard[i]) return 0;
    }
    return 1;
}