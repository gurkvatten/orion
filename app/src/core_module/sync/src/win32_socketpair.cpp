#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#include <stdio.h>
#include <io.h>

static void ensureWinsockInit()
{
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        initialized = true;
    }
}

int
socketpair(int af, int type, int prot, int fd[2])
{
    ensureWinsockInit();
    if (af != AF_UNIX || type != SOCK_STREAM) return -1;
    char temp_path[L_tmpnam];
    if (tmpnam(temp_path) == NULL) return -1;
    int server = socket(af, type, prot);
    if (server == -1) return -1;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, temp_path);
    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        closesocket(server);
        return -1;
    }
    if (listen(server, 1) == -1) {
        closesocket(server);
        _unlink(temp_path);
        return -1;
    }
    int client = socket(af, type, prot);
    if (client == -1) {
        closesocket(server);
        _unlink(temp_path);
        return -1;
    }
    if (connect(client, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        closesocket(client);
        closesocket(server);
        _unlink(temp_path);
        return -1;
    }
    int accepted = accept(server, NULL, NULL);
    if (accepted == -1) {
        closesocket(client);
        closesocket(server);
        _unlink(temp_path);
        return -1;
    }
    closesocket(server);
    _unlink(temp_path);
    fd[0] = client;
    fd[1] = accepted;
    return 0;
}
#endif
