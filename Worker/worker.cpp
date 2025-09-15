#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include "common.h"
#include "queue.h"
#pragma comment(lib, "ws2_32.lib")

#define LB_PORT 6069
#define BUFFER_SIZE 1024

queue* q = NULL;
int main(int argc, char* argv[]) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in lbAddr;
    char buffer[BUFFER_SIZE];
    int bytesRead;
    int sleepTime=10;
    int queueCapacity=10;
    if (argc > 1) {
        sleepTime = atoi(argv[1]); // argument sa komandne linije
    }
    if (argc > 2) {
        queueCapacity = atoi(argv[2]); // drugi argument = max veličina queue-a
    }
    create_queue(queueCapacity);
    SOCKET rpSock;
    struct sockaddr_in rpAddr;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    lbAddr.sin_family = AF_INET;
    lbAddr.sin_port = htons(LB_PORT);
    InetPtonA(AF_INET, "127.0.0.1", &lbAddr.sin_addr);

    rpSock = socket(AF_INET, SOCK_STREAM, 0);
    rpAddr.sin_family = AF_INET;
    rpAddr.sin_port = htons(7079); // npr. 6000, definisi
    InetPtonA(AF_INET, "127.0.0.1", &rpAddr.sin_addr);

    if (connect(rpSock, (struct sockaddr*)&rpAddr, sizeof(rpAddr)) != SOCKET_ERROR) {
        printf("Connected to RP\n");
    }
    else {
        printf("Failed to connect to RP: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&lbAddr, sizeof(lbAddr)) != SOCKET_ERROR) {
        printf("Connected to LB\n");
        printf("Worker started with SleepTime: %d ms\n", sleepTime);

        

        // Worker sada čeka poruke od LB
        while (1) {
            messageStruct msg;
            int bytesRead = recv(sock, (char*)&msg, sizeof(messageStruct), 0);
            if (bytesRead == sizeof(messageStruct)) {
                if (is_queue_full()) {
                    send(sock, "BUSY", 4, 0);
                    printf("Queue is full sending BUSY");
                    continue;
                }
                messageStruct* newMsg = (messageStruct*)malloc(sizeof(messageStruct));
                if (!newMsg) {
                    // handle malloc failure, npr.
                    printf("Memory allocation failed!\n");
                    continue;
                }
                *newMsg = msg; // kopira sadržaj strukture
           
                printf("[WORKER] Received from LB:\n");
                printf("Client: %s\n", msg.clientName);
                printf("Message: %s\n", msg.bufferNoName);
                enqueue(newMsg);

                send(rpSock, (char*)&msg, sizeof(messageStruct), 0);
                Sleep(sleepTime);//simulacija obrade
                // Pošalji odgovor nazad
                printf("QUEUE is full:%d of %d", q->currentSize, q->capacity);

                send(sock, "DONE", 4, 0);
                print_queue();
            }
            else if (bytesRead == 0) {
                printf("[WORKER] Connection closed by LB.\n");
                break;
            }
            else {
                printf("[WORKER] recv failed: %d\n", WSAGetLastError());
                break;
            }
         
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
