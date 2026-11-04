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