#include "communication.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#pragma comment(lib, "wsock32.lib")
#endif

#define BUF_SIZE 65536

/**
 * SSL�ʐM���s��
 * @param ssl_socket �Z�b�V�������
 * @return >0: ���炩�̃��b�Z�[�W�����M����Ă���
 * @return =0: �ؒf
 * @return <0: ���b�Z�[�W�����M����Ă��Ă��Ȃ�
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
        // ���b�Z�[�W�������Ă��Ă��Ȃ��Ƃ���len��-1������
        // �ؒf���ꂽ�ꍇ��0������
        len = SSL_read(ssl, buf, BUF_SIZE);
        if (len < 0);
        else {
            buf[len] = '\0';
            perror(buf);
            // Todo:�����ɏ���
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

