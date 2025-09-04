#include "Common.h"

string SimpleMessage::toString() const {
    return to_string((int)type) + "|" + to_string(workerID) + "|" + data;
}

SimpleMessage SimpleMessage::fromString(const string& str) {
    SimpleMessage msg;
    size_t pos1 = str.find('|');
    size_t pos2 = str.find('|', pos1 + 1);

    if (pos1 != string::npos && pos2 != string::npos) {
        msg.type = (RequestType)stoi(str.substr(0, pos1));
        msg.workerID = stoi(str.substr(pos1 + 1, pos2 - pos1 - 1));
        msg.data = str.substr(pos2 + 1);
    }

    return msg;
}

void NetworkHelper::initNetwork() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void NetworkHelper::cleanupNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

SOCKET NetworkHelper::createTCPSocket() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != INVALID_SOCKET) {
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    }
    return sock;
}

bool NetworkHelper::sendData(SOCKET sock, const string& data) {
    int len = (int)data.length();
    int sent = send(sock, data.c_str(), len, 0);
    return sent == len;
}

string NetworkHelper::receiveData(SOCKET sock) {
    char buffer[1024];
    int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        buffer[received] = '\0';
        return string(buffer);
    }
    return "";
}