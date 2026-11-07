#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>  
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>


#include "helper.h"

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

Buffer *read_file(char *filename) {
    errno = 0;

    int fd = open(filename, O_RDONLY);
    if (fd == -1 || errno != 0) return NULL;

    Buffer *buf = create_buf(BUFLEN);

    int bytes_read = 0;
    char per_line_buf[BUFLEN];
    while ((bytes_read = read(fd, per_line_buf, BUFLEN))) {
        if (bytes_read < 0) {
            free_dyna(buf);
            close(fd);
            return NULL;
        }
        add_to_buf(buf, per_line_buf, bytes_read);
    }
    
    add_null_term(buf);
    
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

int is_dir(char *file_path) {
    struct stat st = {};
    lstat(file_path, &st);
    if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) return 1;
    return 0;
}

int is_reg_file(char *file_path) {
    struct stat st = {};
    lstat(file_path, &st);
    if (!S_ISLNK(st.st_mode) && S_ISREG(st.st_mode)) return 1;
    return 0;
}

int parent_dir(char *file_name) {
    int file_len = strlen(file_name);
    char *parent = "..";
    for (int i = 0; i < file_len+1; i++) {
        if (file_name[i] != parent[i]) return 0;
    }
    return 1;
}

char *request_file_to_path(char *requested_file) {
    int req_len = strlen(requested_file);
    
    Buffer *next_file = create_buf(BUFLEN);
    int added_target = 6;
    add_to_buf(next_file, "public", 7);

    int base = 1;
    for (int i = 1; i < req_len + 1; i++) {
        if (requested_file[i] == '/' || i == req_len) {
            //for file nav
            if (!is_dir(next_file->buf)) {
                free_dyna(next_file);
                return NULL;
            }

            DIR *dir = opendir(next_file->buf);
            if (!dir) {
                free_dyna(next_file);
                return NULL;
            }

            char *file = (char *)malloc(sizeof(char) * (i-base+1));
            memcpy(file, requested_file+base, i-base);
            file[i-base] = '\0';

            if (parent_dir(file)) {
                closedir(dir);
                free_dyna(next_file);
                free(file);
                return NULL;
            }

            struct dirent *dir_struct;
            int found = 0;
            while ((dir_struct = readdir(dir))) {
                if (!strcmp(dir_struct->d_name, file)) {
                    //add the next file in the chain to the shit
                    next_file->buf[added_target] = '/';
                    added_target+=1;
                    add_to_buf(next_file, file, i-base+1);
                    added_target+=i-base;

                    found = 1;
                    break;
                }
            }

            base = i+1;
            free(file);
            closedir(dir);

            if (!found){
                free_dyna(next_file);
                return NULL;
            }
        }
    }

    if (!is_reg_file(next_file->buf)) {
        free_dyna(next_file);
        return NULL;
    } 

    char *reg_buf = (char *)malloc(sizeof(char)*(next_file->size));
    memcpy(reg_buf, next_file->buf, next_file->size);
    free_dyna(next_file);
    return reg_buf;
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
