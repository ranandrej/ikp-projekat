#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

#define LB_PORT 6069
#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in lbAddr;
    char buffer[BUFFER_SIZE];
    int bytesRead;
    int sleepTime;
    if (argc > 1) {
        sleepTime = atoi(argv[1]); // argument sa komandne linije
    }

    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    lbAddr.sin_family = AF_INET;
    lbAddr.sin_port = htons(LB_PORT);
    InetPtonA(AF_INET, "127.0.0.1", &lbAddr.sin_addr);

    if (connect(sock, (struct sockaddr*)&lbAddr, sizeof(lbAddr)) != SOCKET_ERROR) {
        printf("Connected to LB\n");
        printf("Worker started with SleepTime: %d ms\n", sleepTime);

        

        // Worker sada čeka poruke od LB
        while (1) {
            bytesRead = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                printf("[WORKER] Received from LB: %s\n", buffer);

                // možeš ubaciti identifikaciju
                char reply[BUFFER_SIZE];
                snprintf(reply, sizeof(reply), "DONE");

                // šalje nazad LB-u
                DWORD start = GetTickCount();
                Sleep(sleepTime);
                DWORD end = GetTickCount();
                printf("[WORKER] Slept for %u ms\n", end - start);
                fflush(stdout);

                send(sock, reply, strlen(reply), 0);
            }
            else if (bytesRead == 0) {
                printf("LB closed connection\n");
                break;
            }
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
