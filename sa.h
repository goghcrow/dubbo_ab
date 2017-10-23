#ifndef SA_H
#define SA_H

#include <arpa/inet.h>
#include <stdbool.h>

#define SA_BUF_SIZE INET6_ADDRSTRLEN

union sockaddr_all {
    struct sockaddr s;
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;
};

union sockaddr_all sa_create(uint16_t port, bool loopback_only);
union sockaddr_all sa_createV6(uint16_t port, bool loopback_only);
union sockaddr_all sa_fromip(const char *ip, uint16_t port);
union sockaddr_all sa_fromipV6(const char *ip, uint16_t port);

void sa_toip(union sockaddr_all *u, char *buf, int size);
void sa_toipport(union sockaddr_all *u, char *buf, int size);
uint16_t sa_toport(union sockaddr_all *u);
uint32_t sa_iplong(union sockaddr_all *u);
sa_family_t sa_family(union sockaddr_all *u);

bool sa_resolve(char *hostname, union sockaddr_all *u);

#endif