#include "queue.h"

void initQueue(Queue *q) {
    q->front = NULL;
    q->rear = NULL;
}

int isEmpty(Queue *q) {
    return q->front == NULL;
}

void enqueue(Queue *q, void* data_ptr) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("mem alloc error\n");
        return;
    }
    newNode->data_ptr = data_ptr;
    newNode->next = NULL;

    if (isEmpty(q)) {
        q->front = newNode;
        q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

void* dequeue(Queue *q) {
    if (isEmpty(q)) {
        printf("Queue is empty!\n");
        return NULL;
    }
    Node *temp = q->front;
    void* value = temp->data_ptr;
    q->front = temp->next;
    free(temp);
    if (q->front == NULL) {
        q->rear = NULL;
    }
    return value;
}
