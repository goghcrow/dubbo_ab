#ifndef DUBBO_CLIENT_H
#define DUBBO_CLIENT_H

#include <stdbool.h>
#include <sys/time.h>

struct dubbo_args
{
    char *host;
    char *port;
    char *service;
    char *method;
    char *args;   /* JSON */
    char *attach; /* JSON */
    struct timeval timeout;
};

struct dubbo_async_args
{
    struct aeEventLoop *el;
    int pipe_n;
    int req_n;
    bool verbos;
};

bool dubbo_invoke_sync(struct dubbo_args *);
bool dubbo_bench_async(struct dubbo_args *, struct dubbo_async_args *);

#endif