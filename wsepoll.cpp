/**
 * @file wsepoll.h
 * this file is licensed under following
 * https://so-wh.at/entry/20080302/p2
 */

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#include <sys/locking.h>
#include <malloc.h>
#include <errno.h>

#include "wsepoll.h"

#pragma comment (lib, "wsock32.lib")
struct wsepsock {
    SOCKET socket;
    unsigned int events;
};

#define LOCK_FILE "wseplck"

static struct wsepsock epsocks[WSA_MAXIMUM_WAIT_EVENTS];

static int mktmpfn(char* dst, int len, const char* src);
static int epoll_ctl_add(int epfd, SOCKET fd, struct epoll_event* event);
static int epoll_ctl_mod(int epfd, SOCKET fd, struct epoll_event* event);
static int epoll_ctl_del(int epfd, SOCKET fd, struct epoll_event* event);
static int epoll_wait_init(int* nfds, SOCKET* fds, HANDLE* hEvents);
static void epoll_wait_clean(SOCKET* fds, HANDLE* hEvents);

int epoll_create(int size) {
    int i, epfd;
    char tmpfn[MAX_PATH];

    if (!mktmpfn(tmpfn, MAX_PATH, LOCK_FILE)) {
        return -1;
    }

    if (_sopen_s(&epfd, tmpfn, _O_CREAT | _O_TEMPORARY, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0) {
        return -1;
    }

    if (_locking(epfd, _LK_NBLCK, 1) != 0) { return -1; }

    for (i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        epsocks[i].socket = INVALID_SOCKET;
        epsocks[i].events = 0;
    }

    _locking(epfd, _LK_UNLCK, 1);

    return epfd;
}

int epoll_ctl(int epfd, int op, SOCKET fd, struct epoll_event* event) {
    int retval = -1;

    switch (op) {
    case EPOLL_CTL_ADD:
        retval = epoll_ctl_add(epfd, fd, event);
        break;
    case EPOLL_CTL_MOD:
        retval = epoll_ctl_mod(epfd, fd, event);
        break;
    case EPOLL_CTL_DEL:
        retval = epoll_ctl_del(epfd, fd, event);
        break;
    }

    return retval;
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
    int nfds;
    SOCKET fds[WSA_MAXIMUM_WAIT_EVENTS];
    HANDLE hEvents[WSA_MAXIMUM_WAIT_EVENTS];
    DWORD n;

    if (!epoll_wait_init(&nfds, fds, hEvents)) {
        epoll_wait_clean(fds, hEvents);
        return -1;
    }

    n = WSAWaitForMultipleEvents(nfds, hEvents, FALSE, timeout, FALSE);

    if (n == WSA_WAIT_FAILED) {
        epoll_wait_clean(fds, hEvents);
        return -1;
    }
    else if (n == WSA_WAIT_TIMEOUT) {
        epoll_wait_clean(fds, hEvents);
        return 0;
    }
    else {
        SOCKET fd;
        HANDLE hEvent;
        WSANETWORKEVENTS ev;

        n -= WSA_WAIT_EVENT_0;
        fd = fds[n];
        hEvent = hEvents[n];

        if (WSAEnumNetworkEvents(fd, hEvent, &ev) < 0) {
            epoll_wait_clean(fds, hEvents);
            return -1;
        }

        events[0].data.fd = fd;
        events[0].events = ev.lNetworkEvents;
        epoll_wait_clean(fds, hEvents);
        return 1;
    }
}

//-------------------------------------------------------------------

static int mktmpfn(char* dst, int len, const char* src) {
    char* buf = (char*)alloca(sizeof(char) * len);

    if (GetTempPathA(len, buf) == 0) {
        return 0;
    }

    if (GetTempFileNameA(buf, src, 0, dst) == 0) {
        return 0;
    }

    return 1;
}

static int epoll_ctl_add(int epfd, SOCKET fd, struct epoll_event* event) {
    int i;

    if (_locking(epfd, _LK_NBLCK, 1) != 0) { return -1; }

    for (i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        if (epsocks[i].socket == INVALID_SOCKET) {
            epsocks[i].socket = fd;
            epsocks[i].events = event->events;
            break;
        }
    }

    _locking(epfd, _LK_UNLCK, 1);

    return (i < WSA_MAXIMUM_WAIT_EVENTS) ? 0 : -1;
}

static int epoll_ctl_mod(int epfd, SOCKET fd, struct epoll_event* event) {
    int i;

    if (_locking(epfd, _LK_NBLCK, 1) != 0) { return -1; }

    for (i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        if (epsocks[i].socket == fd) {
            epsocks[i].events = event->events;
            break;
        }
    }

    _locking(epfd, _LK_UNLCK, 1);

    return (i < WSA_MAXIMUM_WAIT_EVENTS) ? 0 : -1;
}

static int epoll_ctl_del(int epfd, SOCKET fd, struct epoll_event* event) {
    int i;

    if (_locking(epfd, _LK_NBLCK, 1) != 0) { return -1; }

    for (i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        if (epsocks[i].socket == fd) {
            epsocks[i].socket = INVALID_SOCKET;
            epsocks[i].events = 0;
            break;
        }
    }

    _locking(epfd, _LK_UNLCK, 1);

    return (i < WSA_MAXIMUM_WAIT_EVENTS) ? 0 : -1;
}

static int epoll_wait_init(int* nfds, SOCKET* fds, HANDLE* hEvents) {
    int i;
    (*nfds) = 0;

    for (i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        fds[i] = INVALID_SOCKET;
        hEvents[i] = NULL;
    }

    for (i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        int n = *nfds;
        if (epsocks[i].socket == INVALID_SOCKET) { continue; }

        fds[n] = epsocks[i].socket;
        hEvents[n] = WSACreateEvent();

        if (WSAEventSelect(fds[n], hEvents[n], epsocks[i].events) < 0) {
            errno = WSAGetLastError();
            return 0;
        }

        (*nfds)++;
    }

    return 1;
}

static void epoll_wait_clean(SOCKET* fds, HANDLE* hEvents) {
    int i;

    for (i = 0; i < WSA_MAXIMUM_WAIT_EVENTS; i++) {
        if (fds[i] != INVALID_SOCKET) {
            WSAEventSelect(fds[i], NULL, 0);
        }

        if (hEvents[i] != NULL) {
            WSACloseEvent(hEvents[i]);
        }
    }
}
#endif