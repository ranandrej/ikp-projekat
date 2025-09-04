#define WIN32_LEAN_AND_MEAN      // sprečava uključivanje nepotrebnih Windows definicija
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "includes.h"
#include "common.h"
#include "client_communication.h"




bool is_socket_ready(SOCKET socket, bool isRead) {
	FD_SET set;
	timeval tv;

	FD_ZERO(&set);
	FD_SET(socket, &set);

	tv.tv_sec = 0;
	tv.tv_usec = 50;

	int iResult;

	if (isRead) { //see if socket is ready for READ
		iResult = select(0, &set, NULL, NULL, &tv);
	}
	else {	//see if socket is ready for WRITE
		iResult = select(0, NULL, &set, NULL, &tv);
	}

	if (iResult <= 0)
		return false;
	else
		return true;
}