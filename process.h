#ifndef __PROCESS_H__
#define __PROCESS_H__

#include "linked_list.h"

#define MAX_NAME_LEN 1024
typedef struct
{
    char *name;
    int pid;
} Process;

// define a function to list all processes return an array of Process
LinkedListNode *listProcesses();

#endif // __PROCESS_H__
