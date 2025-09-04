#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN

#include "includes.h"
#include "worker_communication.h"
#include "queue.h"
#include "list.h"

#pragma warning(disable:4996)
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define SERVER_PORT 6069
#define BUFFER_SIZE 256

extern HANDLE semaphoreEnd;
extern list* free_workers_list;
extern queue* q;

// ------------------- WORKER WRITE ------------------- //
// Svaki worker ima svoju nit za slanje poruka koje dođu iz queue-a
DWORD WINAPI worker_write(LPVOID param) {
    node* new_node = (node*)param;
    SOCKET acceptedSocket = new_node->acceptedSocket;
    HANDLE msgSemaphore = new_node->msgSemaphore;

    while (true) {
        // čekaj dok mu ne stigne poruka
        WaitForSingleObject(msgSemaphore, INFINITE);

        messageStruct* msg = new_node->msgStruct;
        if (!msg) continue;

        // formatiramo poruku: ClientName:Message
        char messageBuff[BUFFER_SIZE + 1];
        sprintf(messageBuff, "%s:%s", msg->clientName, msg->bufferNoName);

        int iResult = send(acceptedSocket, messageBuff, (int)strlen(messageBuff), 0);
        if (iResult == SOCKET_ERROR) {
            printf("[WORKER WRITE] send failed: %d\n", WSAGetLastError());
            break;
        }

        printf("[WORKER WRITE] Sent to worker: %s\n", messageBuff);
    }

    return 0;
}

// ------------------- WORKER LISTENER ------------------- //
// Osluškuje nove workere i startuje njihovu write nit
DWORD WINAPI worker_listener(LPVOID param) {
    SOCKET listenSocket = INVALID_SOCKET;
    SOCKET acceptedSocket = INVALID_SOCKET;
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        printf("socket failed: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    if (bind(listenSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("bind failed: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    printf("WORKER listener started on port %d...\n", SERVER_PORT);

    while (true) {
        if (WaitForSingleObject(semaphoreEnd, 10) == WAIT_OBJECT_0)
            break;

        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        acceptedSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);

        if (acceptedSocket != INVALID_SOCKET) {
            printf("New Worker connected: %s:%d\n",
                inet_ntoa(clientAddr.sin_addr),
                ntohs(clientAddr.sin_port));

            node* new_node = (node*)malloc(sizeof(node));
            new_node->acceptedSocket = acceptedSocket;
            new_node->msgSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
            new_node->msgStruct = NULL;
            new_node->next = NULL;

            DWORD workerWID;
            new_node->thread_write = CreateThread(NULL, 0, &worker_write, (LPVOID)new_node, 0, &workerWID);

            // Za sada ga samo čuvamo u listi slobodnih workera
            insert_last_node(new_node, free_workers_list);
        }
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}

// ------------------- SIMPLE SENDER ------------------- //
// Uzima poruke iz queue-a i šalje PRVOM workeru
DWORD WINAPI simple_worker_sender(LPVOID param) {
    messageStruct* msg;

    while (true) {
        if (WaitForSingleObject(semaphoreEnd, 10) == WAIT_OBJECT_0)
            break;

        if (!is_queue_empty() && free_workers_list->head != NULL) {
            dequeue(&msg);

            node* w = free_workers_list->head; // uvek prvi worker
            w->msgStruct = msg;
            ReleaseSemaphore(w->msgSemaphore, 1, NULL);
        }

        Sleep(50);
    }

    return 0;
}
