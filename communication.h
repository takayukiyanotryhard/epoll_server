#ifndef _COMMUNICATION_H_
#define _COMMUNICATION_H_
#ifdef _WIN32
#include <WinSock2.h>
#include "wsepoll.h"
#include "sys_queue.h"
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct str_ssl_socket {
    SLIST_ENTRY(str_ssl_socket) entry;
    SSL *ssl;
    SOCKET sock;
    int state;
}ssl_socket_t;

typedef SLIST_HEAD(str_ssl_socket_header, str_ssl_socket)
ssl_socket_header_t;

int communicate(ssl_socket_t* ssl_socket);
void configure_context(SSL_CTX* ctx);
SSL_CTX* create_context();

#endif
