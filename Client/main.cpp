#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma warning(disable:4996)

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 5059
#define BUFFER_SIZE 256

volatile bool running = true;

// Proverava da li je socket spreman za čitanje ili pisanje
bool is_socket_ready(SOCKET socket, bool isRead) {
    FD_SET set;
    timeval tv;

    FD_ZERO(&set);
    FD_SET(socket, &set);

    tv.tv_sec = 0;
    tv.tv_usec = 50;

    int iResult;
    if (isRead)
        iResult = select(0, &set, NULL, NULL, &tv);
    else
        iResult = select(0, NULL, &set, NULL, &tv);

    return (iResult > 0);
}

// Thread za primanje poruka od LB/workera
DWORD WINAPI client_read(LPVOID param) {
    SOCKET connectedSocket = (SOCKET)param;
    char dataBuffer[BUFFER_SIZE];

    while (running) {
        if (!is_socket_ready(connectedSocket, true)) {
            Sleep(10);
            continue;
        }

        int iResult = recv(connectedSocket, dataBuffer, BUFFER_SIZE, 0);

        if (iResult > 0) {
            dataBuffer[iResult] = '\0';
            printf("[CLIENT]: Received: %s\n", dataBuffer);
        }
        else if (iResult == 0) {
            printf("[CLIENT]: Connection closed by server.\n");
            running = false;
            break;
        }
        else {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                printf("[CLIENT]: recv failed with error: %d\n", WSAGetLastError());
                running = false;
                break;
            }
        }
    }

    return 0;
}

int main() {
    SOCKET connectSocket = INVALID_SOCKET;
    WSADATA wsaData;
    char dataBuffer[BUFFER_SIZE];

    // Initialize WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed with error: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Server address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_IP_ADDRESS);
    serverAddress.sin_port = htons(SERVER_PORT);

    // Connect to server
    if (connect(connectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Unable to connect to server.\n");
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    // Set non-blocking mode
    u_long non_blocking = 1;
    ioctlsocket(connectSocket, FIONBIO, &non_blocking);

    // Start read thread
    HANDLE hClientListener;
    DWORD clientID;
    hClientListener = CreateThread(NULL, 0, &client_read, (LPVOID)connectSocket, 0, &clientID);

    // Main loop – unos poruka sa konzole
    while (running) {
        printf("Enter message to send (or 'exit' to quit): ");
        gets_s(dataBuffer, BUFFER_SIZE);

        if (strcmp(dataBuffer, "exit") == 0) {
            running = false;
            break;
        }

        // čekaj dok socket ne bude spreman za pisanje
        while (!is_socket_ready(connectSocket, false)) {
            Sleep(10);
        }

        int iResult = send(connectSocket, dataBuffer, (int)strlen(dataBuffer), 0);
        if (iResult == SOCKET_ERROR) {
            printf("Send failed with error: %d\n", WSAGetLastError());
            running = false;
            break;
        }

        printf("Message successfully sent. Total bytes: %d\n", iResult);
    }

    // Signal thread da završi i čekaj ga
    running = false;
    WaitForSingleObject(hClientListener, INFINITE);
    CloseHandle(hClientListener);

    // Shutdown socket
    shutdown(connectSocket, SD_BOTH);
    closesocket(connectSocket);
    WSACleanup();

    printf("Client exited cleanly.\n");
    return 0;
}
