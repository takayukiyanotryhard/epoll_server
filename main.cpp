#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <io.h>
#include "wsepoll.h"

#include <memory.h>
#include <vector>
#ifdef _WIN32
//#pragma comment (lib, "wsock32.lib")
#pragma comment (lib, "Ws2_32.lib")
#endif

#define STR(var) #var
#define TOSTRING(n) STR(n)
#define PORT_NUM 7
#define PORT_STR TOSTRING(PORT_NUM)
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
    SOCKET sock, socks[MAX_BACKLOG] = { 0 };
    struct addrinfo hints = { 0 };
    struct addrinfo* ai0, * ai;
    struct epoll_event event = { 0 };
    int cnt = 0;
    bool new_comer;

    wsa_startup(MAKEWORD(2, 0));

    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, PORT_STR, &hints, &ai0);

    if ((epfd = epoll_create(MAX_EVENTS)) < 0) {
        die("epoll_create()\n");
    }

    event.events = EPOLLIN;
    for (ai = ai0; ai; ai = ai->ai_next) {
        if ((sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == INVALID_SOCKET) {
            die("socket(): %d\n", WSAGetLastError());
        }

        if (bind(sock, ai->ai_addr, ai->ai_addrlen) != 0) {
            die("bind(): %d\n", WSAGetLastError());
        }

        if (listen(sock, MAX_BACKLOG) != 0) {
            die("listen(): %d\n", WSAGetLastError());
        }

        event.data.fd = sock;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event) < 0) {
            die("epoll_ctl()\n");
        }
        socks[cnt++] = sock;
    }

    while (1) {
        int i, nfds;
        struct addrinfo sa;
        socklen_t len = sizeof(sa);
        struct epoll_event events[MAX_EVENTS];

        if ((nfds = epoll_wait(epfd, events, MAX_EVENTS, EPOLL_TIMEOUT)) < 0) {
            die("epoll_wait()\n");
        }

        for (i = 0; i < nfds; i++) {
            SOCKET fd = events[i].data.fd;
            new_comer = false;
            for (int j = 0; j < MAX_BACKLOG; j++) {
                sock = socks[j];
                if (sock == fd) {
                    new_comer = true;
                    break;
                }
            }

            if (new_comer) {
                int s;

                if ((s = accept(sock, (struct sockaddr*)&sa, &len)) < 0) {
                    error("accept() %d\n", WSAGetLastError());
                    continue;
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
    for (int i = 0; i < MAX_BACKLOG; i++) {
        if (socks[i]) closesocket(socks[i]);
    }
    WSACleanup();
    return 0;
}