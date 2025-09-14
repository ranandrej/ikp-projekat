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
void createWorker(int sleepTimeMs,int queueCapacity) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; // koristi show window flag
    si.wShowWindow = SW_SHOW;
    ZeroMemory(&pi, sizeof(pi));

    wchar_t cmd[128];
    swprintf(cmd, 128, L"Worker.exe %d %d", sleepTimeMs,queueCapacity); // Unicode string

    if (!CreateProcess(
        NULL,   // aplikacija – NULL jer koristimo command line
        cmd,    // komandna linija sa argumentom
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        printf("CreateProcess failed (%d).\n", GetLastError());
    }
    else {
        printf("Worker started with sleep=%d,and capacity=%d ms\n", sleepTimeMs,queueCapacity);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}


int main() {
    DWORD listenerClientID;
    DWORD listenerWorkerID;
    DWORD dispatcherID;

    semaphoreEnd = CreateSemaphore(0, 0, 4, NULL);
    create_queue(20);
    init_list(&free_workers_list);
    init_list(&busy_workers_list);
    createWorker(0,2);
    createWorker(0,10);

   

    printf("Starting Load Balancer...\n");
    HANDLE hClientListener = CreateThread(NULL, 0,&client_listener, (LPVOID)0,0,&listenerClientID);
    HANDLE hListenerWorker = CreateThread(NULL, 0, &worker_listener, (LPVOID)0, 0, &listenerWorkerID);
    HANDLE hRoundRobinDispatcher = CreateThread(NULL, 0, &round_robin_dispatcher, (LPVOID)0, 0, &dispatcherID);

    printf("Press any key to exit:\n");
    getchar();
    getchar();

    ReleaseSemaphore(semaphoreEnd, 1, NULL);

    //Ceka da se zavrsi thread za klijenta
    if (hClientListener)
        WaitForSingleObject(hClientListener, INFINITE);

    return 0;
}
