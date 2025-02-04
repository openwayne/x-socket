#include "linked_list.h"

// create a new node
LinkedListNode *createNode(void *data) {
    LinkedListNode *newListNode = (LinkedListNode *)malloc(sizeof(LinkedListNode));
    if (newListNode == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    newListNode->data = data;
    newListNode->next = NULL;
    return newListNode;
}

// insert a node at the beginning of the list
void insertAtBeginning(LinkedListNode **head, void *data) {
    LinkedListNode *newListNode = createNode(data);
    newListNode->next = *head;
    *head = newListNode;
}

// insert a node at the end of the list
void insertAtEnd(LinkedListNode **head, void *data) {
    LinkedListNode *newListNode = createNode(data);
    if (*head == NULL) {
        *head = newListNode;
        return;
    }
    LinkedListNode *current = *head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = newListNode;
}

// delete a node with a given key
void deleteNode(LinkedListNode **head, void *data) {
    LinkedListNode *current = *head;
    LinkedListNode *previous = NULL;

    while (current != NULL && current->data != data) {
        previous = current;
        current = current->next;
    }

    if (current != NULL) {
        if (previous == NULL) {
            *head = current->next;
        } else {
            previous->next = current->next;
        }
        free(current);
    }
}

// print the list
void printList(LinkedListNode *head, void (*printData)(void *)) {
    LinkedListNode *current = head;
    while (current != NULL) {
        printf("%p -> \n", current->data);
        if(printData == NULL) {
            printf("Data Point: %p\n", current->data);
        } else {
            printData(current->data);
        }
        printf("\n\n");
        current = current->next;
    }
    printf("NULL\n");
}
