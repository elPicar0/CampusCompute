#include "../include/queue.h"

Node* initNode(int val) {
  Node* newNode = (Node*)malloc(sizeof(Node));
  if (newNode == NULL) return NULL;
  newNode->val = val;
  newNode->next = NULL;
  return newNode;
}

Queue* initQueue() {
  Queue* q = (Queue*)malloc(sizeof(Queue));
  if (q == NULL) return NULL;
  q->front = NULL;
  q->rear = NULL;
  q->size = 0;
  return q;
}

void enqueue(Queue** q, int val) {
  Queue* queue = *q;
  if (queue == NULL) return;

  Node* newNode = initNode(val);
  if (newNode == NULL) return;

  if (queue->rear == NULL) {
    queue->front = newNode;
    queue->rear = newNode;
  }
  else {
    queue->rear->next = newNode;
    queue->rear = newNode;
  }
  queue->size++;
}

int dequeue(Queue** q) {
  Queue* queue = *q;
  if (queue == NULL || queue->front == NULL)
    return -1;

  Node* toBeDel = queue->front;
  int delVal = toBeDel->val;

  queue->front = toBeDel->next;
  if (queue->front == NULL)          
    queue->rear = NULL;

  free(toBeDel);
  queue->size--;
  return delVal;
}

int QueueSize(Queue** q) {
  return (*q)->size;
}

int isEmpty(Queue* q) {
  return q == NULL || q->size == 0;
}

void freeQueue(Queue** q) {
  while (!isEmpty(*q)) dequeue(q);
  free(*q);
  *q = NULL;
}
