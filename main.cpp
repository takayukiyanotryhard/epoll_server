#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <io.h>
#include "wsepoll.h"

#include <memory.h>

#ifdef _WIN32
//#pragma comment (lib, "wsock32.lib")
#pragma comment (lib, "Ws2_32.lib")
#endif

#define ECHO_PORT 7
#define MAX_BACKLOG 5
#define RCVBUFSIZE 256
#define MAX_EVENTS WSA_MAXIMUM_WAIT_EVENTS
#define EPOLL_TIMEOUT 1000

#define die(...) do { fprintf(stderr, __VA_ARGS__); WSACleanup(); exit(1); } while(0)
#define error(...) do { fprintf(stderr, __VA_ARGS__); } while(0)

static void wsa_startup(WORD version) {
    int n;
    WSADATA wsaData;

    if ((n = WSAStartup(version, &wsaData)) != 0) {
        die("WASStartup(): %d\n", n);
    }

    if (version != wsaData.wVersion) {
        die("WASStartup(): WinSock version %d.%d not supported\n", LOWORD(version), HIWORD(version));
    }
}

int echo(SOCKET sock) {
    char buf[RCVBUFSIZE + 1];
    size_t len;

    if ((len = recv(sock, buf, RCVBUFSIZE, 0)) < 0) {
        error("recv(): %d\n", WSAGetLastError());
        return -1;
    }

    if (len == 0) { return 0; }

    buf[len] = '\0';
    error("recv: %d '%s'\n", sock, buf);

    if (send(sock, buf, len, 0) < 0) {
        error("send(): %d\n", WSAGetLastError());
        return -1;
    }

    return len;
}


int main() {
    int epfd;
    SOCKET sock;
    struct sockaddr_in addr;
    struct epoll_event event;

    wsa_startup(MAKEWORD(2, 0));

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        die("socket(): %d\n", WSAGetLastError());
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        die("bind(): %d\n", WSAGetLastError());
    }

    if (listen(sock, MAX_BACKLOG) != 0) {
        die("listen(): %d\n", WSAGetLastError());
    }

    if ((epfd = epoll_create(MAX_EVENTS)) < 0) {
        die("epoll_create()\n");
    }

    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = sock;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event) < 0) {
        die("epoll_ctl()\n");
    }

    while (1) {
        int i, nfds;
        struct sockaddr_in sa;
        socklen_t len = sizeof(sa);
        struct epoll_event events[MAX_EVENTS];

        if ((nfds = epoll_wait(epfd, events, MAX_EVENTS, EPOLL_TIMEOUT)) < 0) {
            die("epoll_wait()\n");
        }

        for (i = 0; i < nfds; i++) {
            SOCKET fd = events[i].data.fd;

            if (sock == fd) {
                int s;

                if ((s = accept(sock, (struct sockaddr*)&sa, &len)) < 0) {
                    die("accept()\n");
                }

                error("accepted.\n");

                event.events = EPOLLIN;
                event.data.fd = s;

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, s, &event) < 0) {
                    die("epoll_ctl()\n");
                }
            }
            else {
                if (echo(fd) < 1) {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &event) < 0) {
                        die("epoll_ctl()\n");
                    }

                    closesocket(fd);
                }
            }
        }
    }

    _close(epfd);

    closesocket(sock);
    WSACleanup();
    return 0;
}