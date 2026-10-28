#include <stdio.h>
#include <unistd.h>


#include "config.h"
#include "helper.h"

#define WORKER_NUM_STRING 1
#define WORKER_NUM_ERR 2
#define WORKER_TIMEOUT_STRING 3
#define WORKER_TIMEOUT_ERR 4
#define INCORRECT_OPT 5

void init_config_defaults(Config *cf) {
    cf->timeout = 60;
    cf->workers = 4;
}
void print_err(int err) {
    char *string;
    switch(err) {
        case WORKER_NUM_ERR:
            string = "Please enter amount less than 50 and more than 0";
            break;
        case WORKER_NUM_STRING:
            string = "Please enter valid positive int for number of workers";
            break;
        case WORKER_TIMEOUT_ERR:
            string = "Please enter amount more than 5 and less than 960 seconds for timeout option";
            break;
        case WORKER_TIMEOUT_STRING:
            string = "Please valid positive int for timeout time";
            break;
        default:
            string = "Unknown error occurred";
    }

    printf("%s\n", string);
}

void printHelp() {
    printf("You are using C web server with the following expected format:\n\
        ./server.exe [framework entry file] [optional args] \n\
        (include -n for num workers and -t for timeout time)\n");
}

int main(int argc, char *argv[]) {
    if (argc > 5) {
        fprintf(stderr, "Bro thinks there are more than 3 args");
        return 1;
    }

    Config cf = {};
    init_config_defaults(&cf);
    
    int option;
    while ((option = getopt(argc, argv, "n:t:h")) != -1) {
        switch(option) {
            case 'n':
                int workers = int_from_string(optarg);
                if (workers == -1) print_err(WORKER_NUM_STRING);
                else if (workers < 1 || workers > 50) print_err(WORKER_NUM_ERR);
                
                if (workers <= 0) return 1;
                cf.workers = workers;
                break;
            case 't':
                int timeout = int_from_string(optarg);
                if (timeout == -1) print_err(WORKER_TIMEOUT_STRING);
                else if (timeout < 5 || timeout > 960) print_err(WORKER_TIMEOUT_ERR);

                if (timeout < 5 || timeout > 960) return 1;
                cf.timeout = timeout;
                break;
            case 'h':
                printHelp();
                return 0;
            default:
                print_err(INCORRECT_OPT);
                return 1;
        }
    }
}