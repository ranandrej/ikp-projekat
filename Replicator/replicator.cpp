#include "replicator.h"
#include "list.h"
#include "queue.h"
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define REPLICATOR_PORT 7079

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

            DWORD replicatorWID;
            new_node->thread_write = CreateThread(NULL, 0, &replicator_write, (LPVOID)new_node, 0, &replicatorWID);

            // Dodajemo u listu svih WR-a koji šalju RP
            //insert_last_node(replica_workers_list, new_node);
        }
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
DWORD WINAPI replicator_write(LPVOID param) {
    node* wr_node = (node*)param;

    while (true) {
        char buffer[1024];
        int bytesReceived = recv(wr_node->acceptedSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) break;
        buffer[bytesReceived] = '\0';
        // lock mutex pre upisa u RP storage
        printf("WORKER sent:%s\n",buffer);
        print_queue();
      
    }

    closesocket(wr_node->acceptedSocket);
    return 0;
}
