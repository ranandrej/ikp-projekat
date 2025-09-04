#define WIN32_LEAN_AND_MEAN      // sprečava uključivanje nepotrebnih Windows definicija
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "includes.h"
#include "client_communication.h"
#include "common.h"
#include "queue.h"
#pragma warning(disable:4996)
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define BUFFER_SIZE 256
#define CLIENT_NAME_LEN 10 
#define SERVER_PORT 5059
#define _WINSOCK_DEPRECATED_NO_WARNINGS

static int client_count = 0;//brojac koliko klijenata se povezalo

struct clientThreadStruct {
    SOCKET acceptedSocket;
    char clientName[CLIENT_NAME_LEN];
};// Struktura potrebna za kreirenje niti koja prihvata 1 klijenta,njegovo ime i SOCKET



//funkcija za kreiranje niti za citanje poruka od klijenta i ispis(obradu)
DWORD WINAPI client_read_write(LPVOID param) {
    clientThreadStruct* paramStruct = (clientThreadStruct*)param;
    SOCKET acceptedSocket = paramStruct->acceptedSocket;
    char clientName[CLIENT_NAME_LEN];
    sprintf(clientName, paramStruct->clientName);

    u_long non_blocking = 1;
    ioctlsocket(acceptedSocket, FIONBIO, &non_blocking);//postavljamo socket dodeljen klijentu u neblokirajuci rezim ako bi radili razlicite operacije u istom threadu
    char dataBuffer[BUFFER_SIZE];
    int client_num = client_count++;

    do {
        while (!is_socket_ready(acceptedSocket, true)) {}
        int iResult = recv(acceptedSocket, dataBuffer, BUFFER_SIZE, 0);
        if (iResult > 0) { //ako nije WSA error ispisuje poruku
            dataBuffer[iResult] = '\0';
            printf("Client %d sent: %s.\n", client_num, dataBuffer);
            messageStruct* newMessageStruct = (messageStruct*)malloc(sizeof(messageStruct));
            strcpy(newMessageStruct->clientName, clientName);
            strcpy(newMessageStruct->bufferNoName, dataBuffer);

            enqueue(newMessageStruct);
            if (strcmp(dataBuffer, "exit") == 0) {
                printf("Connection with client %d closed.\n", client_num);
                break;
            }
            
        }
    } while (true);

    int iResult = shutdown(acceptedSocket, SD_BOTH);

    //// Check if connection is succesfully shut down.
    if (iResult == SOCKET_ERROR)
    {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(acceptedSocket);
        WSACleanup();
        return 1;
    }

    closesocket(acceptedSocket);

    return 0;



}
//Funkcija za kreiranje niti koja ceka klijente i kreira konekcije sa njima
DWORD WINAPI client_listener(LPVOID param) {
    SOCKET listenSocket = INVALID_SOCKET;
    SOCKET acceptedSocket = INVALID_SOCKET;
    WSADATA wsaData;

    // Init WSA
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed with error: %d\n", WSAGetLastError());
        return 1;
    }

    // Adresa servera (load balancer-a)
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    // Kreiraj socket
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Bind
    if (bind(listenSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Listen
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    printf("Load Balancer is listening on port %d...\n", SERVER_PORT);

    int client_num = 0;

    // Glavna petlja za prihvatanje konekcija
    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        acceptedSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (acceptedSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            continue;
        }

        printf("New client connected: %s:%d\n",
            inet_ntoa(clientAddr.sin_addr),
            ntohs(clientAddr.sin_port));

        // Popuni strukturu za novi klijent thread
        clientThreadStruct* cli = (clientThreadStruct*)malloc(sizeof(clientThreadStruct));
        cli->acceptedSocket = acceptedSocket;
        sprintf(cli->clientName, "Client%d", client_num++);

        // Startuj thread za komunikaciju sa tim klijentom
        DWORD threadId;
        HANDLE hThread = CreateThread(NULL, 0, client_read_write, (LPVOID)cli, 0, &threadId);

        if (hThread == NULL) {
            printf("CreateThread failed with error: %d\n", GetLastError());
            closesocket(acceptedSocket);
            free(cli);
        }
        else {
            CloseHandle(hThread); // ne moramo da držimo handle
        }
    }

    closesocket(listenSocket);
    WSACleanup();

    return 0;
}
