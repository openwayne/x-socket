#include "process.h"

void printData(void *data) {
    Process *process = (Process *)data;
    printf("PID: %d, Name: %s\n", process->pid, process->name);
}

int main() {
    LinkedListNode* linkedListNode = listProcesses();
    printf("Processes:\n");
    printList(linkedListNode, printData);
    printf("\n\n\n");
    return 0;
}
