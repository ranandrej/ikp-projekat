#include "replicator.h"
#include "list.h"
#include "queue.h"
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define REPLICATOR_PORT 7079

WorkerData* workersListHead = NULL;

void add_message_to_worker(WorkerData* worker, messageStruct* newMsg) {
    MessageNode* newNode = (MessageNode*)malloc(sizeof(MessageNode));
    if (!newNode) {
        printf("Memory allocation failed for MessageNode!\n");
        return;
    }
    newNode->msg = newMsg;
    newNode->next = NULL;

    if (worker->msgTail == NULL) {
        // lista prazna
        worker->msgHead = worker->msgTail = newNode;
    }
    else {
        worker->msgTail->next = newNode;
        worker->msgTail = newNode;
    }
}
void print_all_workers_and_messages(WorkerData* head) {
    WorkerData* currentWorker = head;
    while (currentWorker != NULL) {
        printf("Worker: %s:%d\n", currentWorker->workerNode->ip, currentWorker->workerNode->port);

        MessageNode* currentMsg = currentWorker->msgHead;
        while (currentMsg != NULL) {
            printf("\tClient: %s\n", currentMsg->msg->clientName);
            printf("\tMessage: %s\n\n", currentMsg->msg->bufferNoName);
            currentMsg = currentMsg->next;
        }
        currentWorker = currentWorker->next;
    }
}
void free_message_list(MessageNode* head) {
    MessageNode* current = head;
    while (current != NULL) {
        MessageNode* next = current->next;
        free(current->msg);  
        free(current);
        current = next;
    }
}



DWORD WINAPI replicator_listener(LPVOID param) {
    SOCKET listenSocket = INVALID_SOCKET;
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(REPLICATOR_PORT);

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) { WSACleanup(); return 1; }

    if (bind(listenSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        closesocket(listenSocket); WSACleanup(); return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket); WSACleanup(); return 1;
    }

    printf("REPLICATOR listener started on port %d...\n", REPLICATOR_PORT);

    while (true) {
        if (WaitForSingleObject(semaphoreEnd, 10) == WAIT_OBJECT_0)
            break;

        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        SOCKET acceptedSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (acceptedSocket != INVALID_SOCKET) {
           
            char ip[16];
            InetNtopA(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));

            printf("New WR connected to Replikator: %s:%d\n",
                ip,
                ntohs(clientAddr.sin_port));

            node* new_node = (node*)malloc(sizeof(node));
            new_node->acceptedSocket = acceptedSocket;
            new_node->msgStruct = NULL;  // ovde ćeš čuvati primljene podatke
            new_node->next = NULL;
            new_node->port = ntohs(clientAddr.sin_port);
            InetNtopA(AF_INET, &clientAddr.sin_addr, new_node->ip, sizeof(new_node->ip));

            WorkerData* newWorkerData = (WorkerData*)malloc(sizeof(WorkerData));

            newWorkerData->workerNode = new_node;
            newWorkerData->msgHead = NULL;
            newWorkerData->msgTail = NULL;
            newWorkerData->next = NULL;
            InitializeCriticalSection(&newWorkerData->cs);
            if (workersListHead == NULL) {
                workersListHead = newWorkerData;
            }
            else {
                WorkerData* temp = workersListHead;
                while (temp->next != NULL) {
                    temp = temp->next;
                }
                temp->next = newWorkerData;
            }


            // Ubaci ga u listu svih WorkerData (npr. na kraj)


            DWORD replicatorWID;
            new_node->thread_write = CreateThread(NULL, 0, &replicator_write, (LPVOID)newWorkerData, 0, &replicatorWID);

            // Dodajemo u listu svih WR-a koji šalju RP
        }
        
        

        
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
DWORD WINAPI replicator_write(LPVOID param) {
    WorkerData* wr_node = (WorkerData*)param;

    while (true) {
        messageStruct* newMsg = (messageStruct*)malloc(sizeof(messageStruct));
        if (!newMsg) {
            printf("Memory allocation failed!\n");
            break;
        }

        int bytesReceived = recv(wr_node->workerNode->acceptedSocket, (char*)newMsg, sizeof(messageStruct), 0);
        if (bytesReceived <= 0) {
            free(newMsg);
            break;
        }
        if (bytesReceived < sizeof(messageStruct)) {
            printf("Partial message received, ignoring\n");
            free(newMsg);
            continue;
        }

        EnterCriticalSection(&wr_node->cs); // pretpostavljam da imaš kritičnu sekciju u WorkerData

        add_message_to_worker(wr_node, newMsg);

        LeaveCriticalSection(&wr_node->cs);

        print_all_workers_and_messages(workersListHead);
    }
    
    closesocket(wr_node->workerNode->acceptedSocket);
    EnterCriticalSection(&wr_node->cs);
    free_message_list(wr_node->msgHead);
    LeaveCriticalSection(&wr_node->cs);
    DeleteCriticalSection(&wr_node->cs);

    free(wr_node->workerNode);
    free(wr_node);
    return 0;
}

