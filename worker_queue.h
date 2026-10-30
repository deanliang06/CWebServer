typedef struct Signal {
    int signum;
    struct Signal *next;
} Signal;

typedef struct SignalQueue {
    Signal *head;
    Signal *tail;
} SignalQueue;

void add_queue(SignalQueue *queue, int signum);

int pop_queue(SignalQueue *queue);

int empty(SignalQueue *queue);