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



node* get_next_worker(list* l) {
    EnterCriticalSection(&l->cs);

    if (l->head == NULL) {
        LeaveCriticalSection(&l->cs);
        return NULL;
    }

    if (l->current == NULL) {
        // prvi put – postavi current na head
        l->current = l->head;
    }
    else if (l->current->next == NULL) {
        // ako smo na kraju liste – vrati na početak
        l->current = l->head;
    }
    else {
        // pomeri na sledećeg
        l->current = l->current->next;
    }

    node* result = l->current;
    
    printf("[GET_NEXT] Selected worker %s:%d from free list\n", result->ip, result->port);


    LeaveCriticalSection(&l->cs);
    return result;
}

// ------------------- WORKER WRITE ------------------- //
// Svaki worker ima svoju nit za slanje poruka koje dođu iz queue-a
DWORD WINAPI worker_write(LPVOID param) {
    node* new_node = (node*)param;
    SOCKET acceptedSocket = new_node->acceptedSocket;
    HANDLE msgSemaphore = new_node->msgSemaphore;
    char recvBuff[BUFFER_SIZE];

    while (true) {
        // čekaj dok mu ne stigne poruka
        WaitForSingleObject(msgSemaphore, INFINITE);

        messageStruct* msg = new_node->msgStruct;
        if (!msg) continue;

        // formatiramo poruku: ClientName:Message
        char messageBuff[BUFFER_SIZE + 1];
        sprintf(messageBuff, "%s:%s", msg->clientName, msg->bufferNoName);

        int iResult = send(acceptedSocket, messageBuff, (int)strlen(messageBuff), 0);
        printf("[WORKER %s:%d] Sent: %s\n", new_node->ip, new_node->port, messageBuff);

        if (iResult == SOCKET_ERROR) {
            printf("[WORKER WRITE] send failed: %d\n", WSAGetLastError());
            break;
        }

        iResult = recv(acceptedSocket, recvBuff, BUFFER_SIZE, 0);
        if (iResult > 0) {
            recvBuff[iResult] = '\0';
        

            if (strcmp(recvBuff, "DONE") == 0) {
               
                // Worker završio – vrati ga u free listu
                move_specific_node(free_workers_list, busy_workers_list, new_node);
                printf("[WORKER %s:%d] DONE received -> moving back to free\n", new_node->ip, new_node->port);
                printf("[STATE AFTER DONE]\n");
                printf("  Free: "); print_list(free_workers_list);
                printf("  Busy: "); print_list(busy_workers_list);
                new_node->msgStruct = NULL;
            }
        }
        else if (iResult == 0) {
            printf("[WORKER WRITE] Worker disconnected.\n");
            break;
        }
        else {
            printf("[WORKER WRITE] recv failed: %d\n", WSAGetLastError());
            break;
        }
    }

    return 0;
}

// ------------------- WORKER LISTENER ------------------- //
// Osluškuje nove workere i startuje njihovu write nit
DWORD WINAPI worker_listener(LPVOID param) {
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

        SOCKET acceptedSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (acceptedSocket != INVALID_SOCKET) {
            printf("New Worker connected: %s:%d\n",
                inet_ntoa(clientAddr.sin_addr),
                ntohs(clientAddr.sin_port));

            node* new_node = (node*)malloc(sizeof(node));
            new_node->acceptedSocket = acceptedSocket;
            new_node->msgSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
            new_node->msgStruct = NULL;
            new_node->next = NULL;
            new_node->port = ntohs(clientAddr.sin_port);
            strcpy(new_node->ip, inet_ntoa(clientAddr.sin_addr));

            DWORD workerWID;
            new_node->thread_write = CreateThread(NULL, 0, &worker_write, (LPVOID)new_node, 0, &workerWID);

            // Dodajemo u listu
            insert_last_node(new_node, free_workers_list);

            // Ispisujemo listu
            print_list(free_workers_list);
        }
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}

// ------------------- SIMPLE SENDER ------------------- //
DWORD WINAPI round_robin_dispatcher(LPVOID param) {
    messageStruct* msg;

    if (free_workers_list->head == NULL) {
        printf("[DISPATCHER] No free workers, waiting...\n");
        
    }
    while (true) {
        if (WaitForSingleObject(semaphoreEnd, 10) == WAIT_OBJECT_0)
            break;
      


        if (!is_queue_empty() && free_workers_list->head != NULL) {
            // uzmi poruku iz reda
            dequeue(&msg);
            printf("[DISPATCHER] Dequeued message: %s\n", msg->bufferNoName);

            // uzmi sledećeg slobodnog workera
            node* w = get_next_worker(free_workers_list);
        
          /*  print_list(free_workers_list);
        
            print_list(busy_workers_list);*/

            if (w) {
                // premesti ga u busy listu
                move_specific_node(busy_workers_list, free_workers_list, w);
                printf("[DISPATCHER] Moving worker %s:%d to busy\n", w->ip, w->port);
                printf("[STATE AFTER MOVE]\n");
                printf("  Free: "); print_list(free_workers_list);
                printf("  Busy: "); print_list(busy_workers_list);
                // pošalji poruku
                w->msgStruct = msg;
                ReleaseSemaphore(w->msgSemaphore, 1, NULL);

               
            }
        }

        Sleep(50);
    }
    
    return 0;
}
