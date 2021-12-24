#include "communication.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#pragma comment(lib, "wsock32.lib")
#endif

#define BUF_SIZE 65536

/**
 * SSL通信を行う
 * @param ssl_socket セッション情報
 * @return >0: 何らかのメッセージが送信されてきた
 * @return =0: 切断
 * @return <0: メッセージが送信されてきていない
 */
int communicate(ssl_socket_t* ssl_socket)
{
    int ssl_ret;
    int ssl_eno;
    SSL* ssl;
    int len = 0;
    char reply[] = "test\n";
    char buf[BUF_SIZE];

    if (!ssl_socket || !ssl_socket->ssl) goto error;

    ssl = ssl_socket->ssl;

    ssl_ret = SSL_accept(ssl);
    ssl_eno = SSL_get_error(ssl, ssl_ret);
    if (ssl_ret <= 0) {
        if (ssl_eno == SSL_ERROR_NONE || SSL_ERROR_WANT_READ) len = 1;
        else {
            perror("aaa");
        }
        ERR_print_errors_fp(stderr);
    }
    else {
        // メッセージが送られてきていないときはlenに-1が入る
        // 切断された場合は0が入る
        len = SSL_read(ssl, buf, BUF_SIZE);
        if (len < 0);
        else {
            buf[len] = '\0';
            perror(buf);
            // Todo:ここに処理
            SSL_write(ssl, reply, strlen(reply));
        }
    }

error:
    return len;
}

void configure_context(SSL_CTX* ctx)
{
    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, "cert/cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "cert/key.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}


SSL_CTX* create_context()
{
    const SSL_METHOD* method;
    SSL_CTX* ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

