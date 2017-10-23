#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include "sa.h"

union sockaddr_all
sa_create(uint16_t port, bool loopback_only)
{
    union sockaddr_all u;
    memset(&u, 0, sizeof(u));
    u.v4.sin_family = AF_INET;
    in_addr_t addr = loopback_only ? INADDR_LOOPBACK : INADDR_ANY;
    u.v4.sin_addr.s_addr = htonl(addr);
    u.v4.sin_port = htons(port);
    return u;
}

union sockaddr_all
sa_createV6(uint16_t port, bool loopback_only)
{
    union sockaddr_all u;
    memset(&u, 0, sizeof(u));
    u.v6.sin6_family = AF_INET6;
    u.v6.sin6_addr = loopback_only ? in6addr_loopback : in6addr_any;
    u.v6.sin6_port = htons(port);
    return u;
}

union sockaddr_all
sa_fromip(const char *ip, uint16_t port)
{
    union sockaddr_all u;
    memset(&u, 0, sizeof(u));
    u.v4.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &(u.v4.sin_addr)) <= 0)
    {
        fprintf(stderr, "ERROR inet_pton %s", ip);
    }
    u.v4.sin_port = htons(port);
    return u;
}

union sockaddr_all
sa_fromipV6(const char *ip, uint16_t port)
{
    union sockaddr_all u;
    memset(&u, 0, sizeof(u));
    u.v6.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, ip, &(u.v6.sin6_addr)) <= 0)
    {
        fprintf(stderr, "ERROR inet_pton %s", ip);
    }
    u.v6.sin6_port = htons(port);
    return u;
}

void sa_toip(union sockaddr_all *u, char *buf, int size)
{
    // 使用 static变量做成不需要buf与size参数的函数?
    // static __thread char buf[INET6_ADDRSTRLEN];
    assert(size >= INET6_ADDRSTRLEN);
    if (u->s.sa_family == AF_INET)
    {
        inet_ntop(AF_INET, &(u->v4.sin_addr), buf, INET_ADDRSTRLEN);
    }
    else if (u->s.sa_family == AF_INET6)
    {
        inet_ntop(AF_INET6, &(u->v6.sin6_addr), buf, INET6_ADDRSTRLEN);
    }
}

void sa_toipport(union sockaddr_all *u, char *buf, int size)
{
    assert(size >= INET6_ADDRSTRLEN);
    sa_toip(u, buf, size);
    size_t end = strlen(buf);
    uint16_t port = ntohs(u->v4.sin_port);
    assert(size > end);
    snprintf(buf + end, size - end, ":%u", port);
}

uint16_t sa_toport(union sockaddr_all *u)
{
    return ntohs(u->v4.sin_port);
}

uint32_t sa_iplong(union sockaddr_all *u)
{
    assert(u->s.sa_family == AF_INET);
    return ntohl(u->v4.sin_addr.s_addr);
}

sa_family_t sa_family(union sockaddr_all *u)
{
    return u->s.sa_family;
}

bool sa_resolve(char *hostname, union sockaddr_all *u)
{
    assert(u);
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, NULL, &hints, &res);
    if (status != 0)
    {
        fprintf(stderr, "ERROR getaddrinfo: %s\n", gai_strerror(status));
        return false;
    }

    // FIXME 遍历检测可用??
    // 只获取链表第一个元素 struct addrinfo *p; for(p = res; p != NULL; p = p->ai_next) { }
    if (res == NULL)
    {
        freeaddrinfo(res);
        false;
    }

    u->v4.sin_family = res->ai_family;
    u->v4.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    #ifdef __APPLE__ 
    u->v4.sin_len = res->ai_addrlen;
    #endif
    freeaddrinfo(res);
    return true;
}