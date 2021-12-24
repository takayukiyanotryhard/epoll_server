/**
 * @file main.cpp
 * this file is licensed under following
 * https://so-wh.at/entry/20080302/p2
 */

#include <stdio.h>
#include <io.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "wsepoll.h"
#endif

#include <memory.h>
#ifdef _WIN32
#include "sys_queue.h"
#else
#include <sys/queue.h>
#endif
#include "communication.h"

#ifdef _WIN32
#pragma comment (lib, "Ws2_32.lib")
#endif

#define PORT_NUM 7
#define STR(var) #var
#define TOSTRING(n) STR(n)
#define PORT_STR TOSTRING(PORT_NUM)

#define MAX_BACKLOG 5
#define RCVBUFSIZE 256
#define MAX_EVENTS WSA_MAXIMUM_WAIT_EVENTS
#define EPOLL_TIMEOUT 1000

#define die(...) do { fprintf(stderr, __VA_ARGS__); WSACleanup(); exit(1); } while(0)
#define error(...) do { fprintf(stderr, __VA_ARGS__); } while(0)

#ifdef _WIN32
#define socketclose(x) closesocket(x)
#define fdclose(x) _close(x)
#else
#define socketclose(x) close(s)
#define fdclose(x) close(x)
#endif

#ifdef _WIN32
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
#endif


int main() {
    int epfd;
    SOCKET sock, socks[MAX_BACKLOG] = { 0 }; // –{“–‚Í2ŒÂ‚¾‚¯‚Å‚æ‚¢
    struct addrinfo hints = { 0 };
    struct addrinfo* ai0, * ai;
    struct epoll_event event = { 0 };
    int cnt = 0;
    bool new_comer;
    SSL* ssl;
    SSL_CTX* ctx;
    ssl_socket_t ssl_sockets[MAX_EVENTS] = {0};
    ssl_socket_t *ssl_socket;
    ssl_socket_header_t head = SLIST_HEAD_INITIALIZER(&head);

#ifdef _WIN32
    wsa_startup(MAKEWORD(2, 0));
#endif

    ctx = create_context();
    configure_context(ctx);

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

    SLIST_INIT(&head);

    while (1) {
        int i, nfds;
        struct addrinfo sa;
        socklen_t len = sizeof(sa);
        struct epoll_event events[MAX_EVENTS] = { 0 };

        if ((nfds = epoll_wait(epfd, events, MAX_EVENTS, EPOLL_TIMEOUT)) < 0) {
            die("epoll_wait()\n");
        }

        for (i = 0; i < nfds; i++) {
            SOCKET current_fd = events[i].data.fd;
            new_comer = false;
            for (int j = 0; j < MAX_BACKLOG; j++) {
                sock = socks[j];
                if (sock == current_fd) {
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
                ssl_socket_t *current = new ssl_socket_t();
                current->ssl = SSL_new(ctx);
                current->sock = s;
                SSL_set_fd(current->ssl, s);
                // QUEUE‚É’Ç‰Á
                SLIST_INSERT_HEAD(&head, current, entry);

                error("accepted.\n");

                event.events = EPOLLIN;
                event.data.fd = s;

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, s, &event) < 0) {
                    die("epoll_ctl()\n");
                }
                ;
            }
            else {
                // QUEUE‚©‚çfd‚ðŒ³‚Éssl‚ðŽæ“¾
                SLIST_FOREACH(ssl_socket, &head, entry) {
                    if (ssl_socket->sock == current_fd) {
                        break;
                    }
                }
                if (communicate(ssl_socket)) {
                    SLIST_REMOVE(&head, ssl_socket, str_ssl_socket, entry);
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, current_fd, &event) < 0) {
                        die("epoll_ctl()\n");
                    }
                    SSL_shutdown(ssl_socket->ssl);
                    SSL_free(ssl_socket->ssl);

                    socketclose(current_fd);
                }
            }
        }
    }

    fdclose(epfd);
    for (int i = 0; i < MAX_BACKLOG; i++) {
        if (socks[i]) socketclose(socks[i]);
    }
    SSL_CTX_free(ctx);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}