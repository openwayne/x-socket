#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>

typedef struct queue_node
{
    void *data_ptr;
    struct queue_node *next;
} Node;

typedef struct
{
    Node *front;
    Node *rear;
} Queue;

void initQueue(Queue *q);
int isEmpty(Queue *q);
void enqueue(Queue *q, void *data_ptr);
void *dequeue(Queue *q);

#endif // QUEUE_H__
