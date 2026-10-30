#include <stdlib.h>

typedef struct Signal {
    int signum;
    struct Signal *next;
} Signal;

typedef struct SignalQueue {
    Signal *head;
    Signal *tail;
} SignalQueue;

void add_queue(SignalQueue *queue, int signum) {
    Signal *newSig = (Signal *)malloc(sizeof(Signal));
    newSig->next = NULL;
    newSig->signum = signum;
    if (queue->tail) {
        queue->tail->next = newSig;
        queue->tail = newSig;
    } else {
        queue->head = newSig;
        queue->tail = newSig;
    }
}

int pop_queue(SignalQueue *queue) {
    if (!queue->head) return -1;
    Signal *returned_signal = queue->head;

    Signal *next_head = queue->head->next;
    if (!next_head) {
        queue->tail = NULL;
    }
    queue->head = next_head;

    return returned_signal->signum;
}

int empty(SignalQueue *queue) {
    if (queue->head) return 1;
    return 0;
}
