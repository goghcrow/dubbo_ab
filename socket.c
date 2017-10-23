#ifndef SOCKET_H
#define SOCKET_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/uio.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "socket.h"
#include "sa.h"

static int socket_ctor(const char *host, const char *port, bool nonblock);
static int socket_create_(bool nonblock);
static int socket_accept_(int sockfd, union sockaddr_all *addr, socklen_t *addrlen, bool nonblock);
static void socket_setNonblock(int sockfd);

int socket_client(const char *host, const char *port)
{
    return socket_ctor(host, port, true);
}

int socket_clientSync(const char *host, const char *port)
{
    return socket_ctor(host, port, false);
}

int socket_server(const char *port)
{
    return socket_ctor(NULL, port, true);
}

int socket_serverSync(const char *port)
{
    return socket_ctor(NULL, port, false);
}

int socket_create()
{
    return socket_create_(true);
}

int socket_createSync()
{
    return socket_create_(false);
}

bool socket_bind(int sockfd, const union sockaddr_all *addr, socklen_t addrlen)
{
    if (bind(sockfd, (struct sockaddr *)addr, addrlen) < 0)
    {
        close(sockfd);
        perror("ERROR bind");
        return false;
    }
    return true;
}

bool socket_listen(int sockfd)
{
    // SOMAXCONN net.core.somaxconn linux 最大backlog值
    if (listen(sockfd, SOMAXCONN) < 0)
    {
        perror("ERROR listen");
        return false;
    }
    return true;
}

int socket_accept(int sockfd, union sockaddr_all *addr, socklen_t *addrlen)
{
    return socket_accept_(sockfd, addr, addrlen, true);
}

int socket_acceptSync(int sockfd, union sockaddr_all *addr, socklen_t *addrlen)
{
    return socket_accept_(sockfd, addr, addrlen, false);
}

int socket_connect(int sockfd, const union sockaddr_all *addr, socklen_t addrlen)
{
    return connect(sockfd, (struct sockaddr *)addr, addrlen);
}

ssize_t socket_read(int sockfd, void *buf, size_t count)
{
    return read(sockfd, buf, count);
}

ssize_t socket_readv(int sockfd, const struct iovec *iov, int iovcnt)
{
    return readv(sockfd, iov, iovcnt);
}

ssize_t socket_write(int sockfd, const void *buf, size_t count)
{
    return write(sockfd, buf, count);
}

void socket_close(int sockfd)
{
    if (close(sockfd) < 0)
    {
        perror("ERROR close");
    }
}

// shutdown后最终还必须要close
void socket_shutdownWrite(int sockfd)
{
    if (shutdown(sockfd, SHUT_WR) < 0)
    {
        perror("ERROR shutdown");
    }
}

size_t socket_sendAllSync(int sockfd, const char *buf, size_t size)
{
    size_t sent = 0;
    while (sent < size)
    {
        ssize_t n = send(sockfd, buf + sent, size - sent, 0);
        if (n == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("ERROR send");
                break;
            }
        }
        sent += n;
    }

    return sent;
}

size_t socket_recvAllSync(int sockfd, char *buf, size_t size)
{
    size_t recved = 0;
    while (recved < size)
    {
        ssize_t n = recv(sockfd, buf + recved, size - recved, 0);
        if (n == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("ERROR recv");
                recved = -1;
                break;
            }
        }
        else if (n == 0)
        {
            break; // EOF
        }
        else
        {
            recved += n;
        }
    }

    return recved;
}

int socket_getError(int sockfd)
{
    int optval;
    int len = sizeof(optval);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, (socklen_t *)&len) < 0)
    {
        return errno;
    }
    else
    {
        return optval;
    }
}

static void socket_setNonblock(int sockfd)
{
    int flag = fcntl(sockfd, F_GETFL, 0);
    if (flag < 0)
    {
        fprintf(stderr, "ERROR fail to fcntl F_GETFL\n");
        return;
    }
    if (fcntl(sockfd, F_SETFL, flag | O_NONBLOCK | O_CLOEXEC) == -1)
    {
        fprintf(stderr, "ERROR fail to fcntl F_SETFL\n");
    }
}

static int socket_create_(bool nonblock)
{
#ifdef __APPLE__
    int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    int sockfd = socket(PF_INET, SOCK_STREAM | (nonblock ? SOCK_NONBLOCK : 0) | SOCK_CLOEXEC, IPPROTO_TCP);
#endif

    if (sockfd < 0)
    {
        perror("ERROR socket");
        return -1;
    }

#ifdef __APPLE__
    if (nonblock)
    {
        socket_setNonblock(sockfd);
    }
#endif

    int yes = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));

    // !! for server
    signal(SIGPIPE, SIG_IGN);
    return sockfd;
}

// 使用 struct sockaddr_storage 来接收, 转成 struct sockaddr 所以需要addr_len
static int socket_accept_(int sockfd, union sockaddr_all *addr, socklen_t *addrlen, bool nonblock)
{
#ifdef __APPLE__
    int connfd = accept(sockfd, (struct sockaddr *)addr, addrlen);
#else
    int connfd = accept4(sockfd, (struct sockaddr *)addr, addrlen, (nonblock ? SOCK_NONBLOCK : 0) | SOCK_CLOEXEC);
#endif
    if (connfd < 0)
    {
        int savedErrno = errno;
        switch (savedErrno)
        {
        case EAGAIN:
        case ECONNABORTED:
        case EINTR:
        case EPROTO:
        case EPERM:
        case EMFILE:
            errno = savedErrno;
            break;
        case EBADF:
        case EFAULT:
        case EINVAL:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case ENOTSOCK:
        case EOPNOTSUPP:
            perror("ERROR accept, unexpected error");
            return -1;
        default:
            perror("ERROR accept, unknown error");
            return -1;
        }
    }

#ifdef __APPLE__
    if (nonblock)
    {
        socket_setNonblock(sockfd);
    }
#endif

    return connfd;
}

// 创建socket_client 或者 socket_server
// server 不能传递host, 自行查找绑定
// server :: socket_createSync(NULL, 9999)
// client :: socket_createSync("www.google.com", 90)
static int socket_ctor(const char *host, const char *port, bool nonblock)
{
    bool isServer = host == NULL;

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* For wildcard IP address */

    // !!! sync for client
    int status = getaddrinfo(host, port, &hints, &servinfo);
    if (status != 0)
    {
        // gai_strerror --> getaddrinfo_strerror
        fprintf(stderr, "ERROR getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    int sockfd = -1;

    // 绑定到第一个能用的
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
#ifdef __APPLE__
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#else
        // linux 内核高版本可以直接设置 SOCK_NONBLOCK | SOCK_CLOEXEC, 节省一次fcntl系统调用
        sockfd = socket(p->ai_family, p->ai_socktype | (nonblock ? SOCK_NONBLOCK : 0) | SOCK_CLOEXEC, p->ai_protocol);
#endif

        if (sockfd == -1)
        {
            perror("ERROR socket");
            continue;
        }

#ifdef __APPLE__
        if (nonblock)
        {
            socket_setNonblock(sockfd);
        }
#endif

        int yes = 1;
        if (isServer)
        {
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
            {
                perror("ERROR setsockopt SO_REUSEADDR");
                close(sockfd);
                return -1;
            }

#ifdef SO_REUSEPORT
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) < 0)
            {
                perror("ERROR setsockopt SO_REUSEPORT");
                close(sockfd);
                return -1;
            }
#endif

            if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) < 0)
            {

                perror("ERROR setsockopt SO_KEEPALIVE");
                close(sockfd);
                return -1;
            }
        }

        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) < 0)
        {
            perror("ERROR setsockopt TCP_NODELAY");
            close(sockfd);
            return -1;
        }

        // SO_REUSEADDR needs to be set before bind().
        // However, not all options need to be set before bind(), or even before connect().
        if (isServer && bind(sockfd, p->ai_addr, p->ai_addrlen) < 0)
        {
            perror("ERROR bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        // 注意bind失败要close
        fprintf(stderr, "ERROR failed to socket and bind\n");
        close(sockfd);
        return -1;
    }

    if (isServer)
    {
        // server 已经不用, client connect 仍然用
        freeaddrinfo(servinfo);
        signal(SIGPIPE, SIG_IGN);
        /*
        if (listen(sockfd, SOMAXCONN) < 0)
        {
            freeaddrinfo(servinfo);
            perror("ERROR listen");
            close(sockfd);
            return -1;
        }
        */
    }
    else
    {
        if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
        {
            perror("ERROR connect");
            freeaddrinfo(servinfo);
            close(sockfd);
            return -1;
        }
        freeaddrinfo(servinfo);
    }

    return sockfd;
}

#endif