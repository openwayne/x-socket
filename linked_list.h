#ifndef __LINKED_LIST_H__
#define __LINKED_LIST_H__

#include <stdio.h>
#include <stdlib.h>

typedef struct LinkedListNode {
    void* data;
    struct LinkedListNode *next;
} LinkedListNode;

LinkedListNode *createNode(void *data);
void insertAtBeginning(LinkedListNode **head, void *data);
void insertAtEnd(LinkedListNode **head, void* data);
void deleteNode(LinkedListNode **head, void* data);
void printList(LinkedListNode *head, void (*printData)(void *));

#endif // __LINKED_LIST_H__
