#include <limits.h>

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