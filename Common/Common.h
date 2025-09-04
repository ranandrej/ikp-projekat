#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

using namespace std;

// Portovi
const int LB_PORT = 5059;
const int RP_PORT = 5060;
const int WORKER_BASE_PORT = 6000;

// Tipovi zahteva
enum RequestType {
    STORE_REQUEST = 1,
    REGISTER_WORKER = 2,
    REPLICATE_DATA = 3
};

// Jednostavna poruka
struct SimpleMessage {
    RequestType type;
    int workerID;
    string data;

    string toString() const {
        return to_string((int)type) + "|" + to_string(workerID) + "|" + data;
    }

    static SimpleMessage fromString(const string& str) {
        SimpleMessage msg;
        size_t pos1 = str.find('|');
        size_t pos2 = str.find('|', pos1 + 1);

        msg.type = (RequestType)stoi(str.substr(0, pos1));
        msg.workerID = stoi(str.substr(pos1 + 1, pos2 - pos1 - 1));
        msg.data = str.substr(pos2 + 1);

        return msg;
    }
};

// Network helper funkcije
class NetworkHelper {
public:
    static void initNetwork();
    static void cleanupNetwork();
    static SOCKET createTCPSocket();
    static bool sendData(SOCKET sock, const string& data);
    static string receiveData(SOCKET sock);
};