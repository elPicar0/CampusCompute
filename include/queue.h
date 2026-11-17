#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

typedef struct _Node {
    int val;
    struct _Node* next;
} Node;

typedef struct _Queue {
    Node* front;
    Node* rear;
    int size;
} Queue;

Node* initNode(int val);
Queue* initQueue();
void enqueue(Queue** q, int val);
int dequeue(Queue** q);          /* returns -1 when empty */
int QueueSize(Queue** q);
int isEmpty(Queue* q);
void freeQueue(Queue** q);       /* frees all nodes and the queue, sets *q = NULL */

#endif
