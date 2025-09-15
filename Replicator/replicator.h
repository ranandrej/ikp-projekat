#pragma once
#include <winsock2.h>        // mora da bude pre windows.h
#include <windows.h>   
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "list.h"
#define CLIENT_NAME_LEN 10
#define BUFFER_WITHOUT_NAME 246
#define BUFFER_SIZE CLIENT_NAME_LEN+BUFFER_WITHOUT_NAME

typedef struct MessageNode {
    messageStruct* msg;
    struct MessageNode* next;
} MessageNode;

typedef struct WorkerData {
    node* workerNode;         // pokazivač na tvoj node
    CRITICAL_SECTION cs;

    MessageNode* msgHead;     // head liste poruka
    MessageNode* msgTail;     // tail liste poruka
    struct WorkerData* next;  // sledeći radnik
} WorkerData;

DWORD WINAPI replicator_listener(LPVOID param);
DWORD WINAPI replicator_write(LPVOID param);