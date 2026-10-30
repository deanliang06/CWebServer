#include <limits.h>
#include <string.h>

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

void init_hfile(char *buf, int num_name) {
    int idx = 0;
    if (num_name == 0) {
        buf[0] = '0';
        idx++;
    }

    while (num_name > 0) {
        int digit = num_name % 10;
        num_name/=10;
        buf[idx] = '0'+digit;
        idx++;
    }
    memcpy(buf+idx, "health", 7);
}