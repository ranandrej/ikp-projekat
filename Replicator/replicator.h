#pragma once
#include <winsock2.h>        // mora da bude pre windows.h
#include <windows.h>   
#include <stdio.h>
#include <stdlib.h>

#define CLIENT_NAME_LEN 10
#define BUFFER_WITHOUT_NAME 246
#define BUFFER_SIZE CLIENT_NAME_LEN+BUFFER_WITHOUT_NAME



DWORD WINAPI replicator_listener(LPVOID param);
DWORD WINAPI replicator_write(LPVOID param);