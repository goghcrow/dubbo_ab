#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#define UNUSED(x) ((void)(x))

#define LOG_INFO(fmt, ...) \
    fprintf(stderr, "\x1B[1;32m[INFO] " fmt "\x1B[0m in function %s %s:%d\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "\x1B[1;31m[ERROR] " fmt "\x1B[0m in function %s %s:%d\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);

#define PANIC(fmt, ...)                                                                                                     \
    fprintf(stderr, "\x1B[1;31m[PANIC] " fmt "\x1B[0m in function %s %s:%d\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__); \
    exit(1);

#endif