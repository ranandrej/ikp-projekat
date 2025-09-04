#pragma once
#include <winsock2.h>        // mora da bude pre windows.h
#include <windows.h>   
#include <stdio.h>
#include <stdlib.h>

#define CLIENT_NAME_LEN 10
#define BUFFER_WITHOUT_NAME 246
#define BUFFER_SIZE CLIENT_NAME_LEN+BUFFER_WITHOUT_NAME

typedef struct messageStruct {
	char clientName[CLIENT_NAME_LEN];
	char bufferNoName[BUFFER_WITHOUT_NAME];
}messageStruct;

extern HANDLE semaphoreEnd;
// read = true  → proverava da li ima podataka za čitanje
// read = false → proverava da li se može pisati
bool is_socket_ready(SOCKET s, bool read);
