#include "replicator.h"
#include <iostream>
#include "queue.h"
#include "replicator.h"
queue* q = NULL;
HANDLE semaphoreEnd;
int main(int argc,char** argv){
    DWORD replicatorID;

    semaphoreEnd = CreateSemaphore(0, 0, 4, NULL);
    create_queue(10);

    printf("Starting Replicator...\n");
    HANDLE hReplicatorListener = CreateThread(NULL, 0, &replicator_listener, (LPVOID)0, 0, &replicatorID);

    printf("Press any key to exit:\n");
    getchar();
    getchar();

    ReleaseSemaphore(semaphoreEnd, 1, NULL);

    return 0;
}
