#define WIN32_LEAN_AND_MEAN      // sprečava uključivanje nepotrebnih Windows definicija
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "includes.h"
#include "queue.h"
#include "client_communication.h"
#include "worker_communication.h"
#include "list.h"
HANDLE semaphoreEnd;

list* free_workers_list = NULL;
list* busy_workers_list = NULL;

queue* q = NULL;

int main() {
    DWORD listenerClientID;
    DWORD listenerWorkerID;
    semaphoreEnd = CreateSemaphore(0, 0, 4, NULL);
    create_queue(20);
    printf("Starting Load Balancer...\n");
    HANDLE hClientListener = CreateThread(NULL, 0,&client_listener, (LPVOID)0,0,&listenerClientID);
    HANDLE hListenerWorker = CreateThread(NULL, 0, &worker_listener, (LPVOID)0, 0, &listenerWorkerID);

    printf("Press any key to exit:\n");
    getchar();
    getchar();

    ReleaseSemaphore(semaphoreEnd, 1, NULL);

    //Ceka da se zavrsi thread za klijenta
    if (hClientListener)
        WaitForSingleObject(hClientListener, INFINITE);

    return 0;
}
