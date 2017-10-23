#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/uio.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>
#include <assert.h>

#include "endian.h"
#include "buffer.h"

struct buffer
{
    size_t read_idx;
    size_t write_idx;
    size_t sz;
    char *buf;
    size_t p_sz;
};

struct buffer *buf_create_ex(size_t size, size_t prepend_size)
{
    assert(prepend_size >= 0);

    if (size == 0)
    {
        size = 1024;
    }
    size_t sz = size + prepend_size;
    struct buffer *buf = malloc(sizeof(*buf));
    assert(buf);
    memset(buf, 0, sizeof(*buf));
    buf->buf = malloc(sz);
    assert(buf->buf);
    memset(buf->buf, 0, sz);
    buf->sz = sz;
    buf->read_idx = prepend_size;
    buf->write_idx = prepend_size;
    buf->p_sz = prepend_size;
    return buf;
}

void buf_release(struct buffer *buf)
{
    free(buf->buf);
    free(buf);
}

size_t buf_getReadIndex(struct buffer *buf)
{
    return buf->read_idx;
}

void buf_setReadIndex(struct buffer *buf, size_t read_idx)
{
    assert(read_idx > 0 && read_idx <= buf->write_idx);
    buf->read_idx = read_idx;
}

size_t buf_getWriteIndex(struct buffer *buf)
{
    return buf->write_idx;
}

void buf_setWriteIndex(struct buffer *buf, size_t write_idx)
{
    assert(write_idx >= buf->read_idx && write_idx < buf->sz);
    buf->write_idx = write_idx;
}

size_t buf_readable(const struct buffer *buf)
{
    return buf->write_idx - buf->read_idx;
}

size_t buf_writable(const struct buffer *buf)
{
    return buf->sz - buf->write_idx;
}

size_t buf_prependable(const struct buffer *buf)
{
    return buf->read_idx;
}

const char *buf_peek(const struct buffer *buf)
{
    return buf->buf + buf->read_idx;
}

char *buf_beginWrite(struct buffer *buf)
{
    return buf->buf + buf->write_idx;
}

void buf_has_written(struct buffer *buf, size_t len)
{
    assert(len <= buf_writable(buf));
    buf->write_idx += len;
}

void buf_unwrite(struct buffer *buf, size_t len)
{
    assert(len <= buf_readable(buf));
    buf->write_idx -= len;
}

const char *buf_findCRLF(struct buffer *buf)
{
    return (char *)memmem(buf_peek(buf), buf_readable(buf), "\r\n", 2);
}

const char *buf_findEOL(struct buffer *buf)
{
    return (char *)memchr(buf_peek(buf), '\n', buf_readable(buf));
}

void buf_retrieveAsString(struct buffer *buf, size_t len, char *str)
{
    assert(str != NULL);
    memcpy(str, buf_peek(buf), len);
    str[len] = 0;
    buf_retrieve(buf, len);
}

void buf_retrieveAll(struct buffer *buf)
{
    buf->read_idx = buf->p_sz;
    buf->write_idx = buf->p_sz;
}

void buf_retrieve(struct buffer *buf, size_t len)
{
    assert(len <= buf_readable(buf));
    if (len < buf_readable(buf))
    {
        buf->read_idx += len;
    }
    else
    {
        buf_retrieveAll(buf);
    }
}

void buf_retrieveUntil(struct buffer *buf, const char *end)
{
    assert(buf_peek(buf) <= end);
    assert(end <= buf_beginWrite(buf));
    buf_retrieve(buf, end - buf_peek(buf));
}

void buf_retrieveInt64(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int64_t));
}

void buf_retrieveInt32(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int32_t));
}

void buf_retrieveInt16(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int16_t));
}

void buf_retrieveInt8(struct buffer *buf)
{
    buf_retrieve(buf, sizeof(int8_t));
}

static void buf_swap(struct buffer *buf, size_t nsz)
{
    // FIX nsz > buf->size realloc ?
    assert(nsz >= buf_readable(buf));
    void *nbuf = malloc(nsz);
    assert(nbuf);
    memset(nbuf, 0, nsz);
    memcpy(nbuf + buf->p_sz, buf_peek(buf), buf_readable(buf));
    free(buf->buf);
    buf->buf = nbuf;
    buf->sz = nsz;
}

static void buf_makeSpace(struct buffer *buf, size_t len)
{
    size_t readable = buf_readable(buf);
    if (buf_prependable(buf) + buf_writable(buf) - buf->p_sz < len)
    {
        size_t nsz = buf->write_idx + len;
        buf_swap(buf, nsz);
    }
    else
    {
        assert(buf->p_sz < buf->read_idx);
        memmove(buf->buf + buf->p_sz, buf_peek(buf), readable);
    }

    buf->read_idx = buf->p_sz;
    buf->write_idx = buf->p_sz + readable;
    assert(readable == buf_readable(buf));
}

void buf_ensureWritable(struct buffer *buf, size_t len)
{
    if (buf_writable(buf) < len)
    {
        buf_makeSpace(buf, len);
    }
    assert(buf_writable(buf) >= len);
}

void buf_append(struct buffer *buf, const char *data, size_t len)
{
    buf_ensureWritable(buf, len);
    memcpy(buf_beginWrite(buf), data, len);
    buf_has_written(buf, len);
}

void buf_prepend(struct buffer *buf, const char *data, size_t len)
{
    assert(len <= buf_prependable(buf));
    buf->read_idx -= len;
    memcpy((void *)buf_peek(buf), data, len);
}

void buf_shrink(struct buffer *buf, size_t reserve)
{
    buf_swap(buf, buf->p_sz + buf_readable(buf) + reserve);
}

void buf_appendInt64(struct buffer *buf, int64_t x)
{
    int64_t be64 = htobe64(x);
    buf_append(buf, (char *)&be64, sizeof(int64_t));
}

void buf_appendInt32(struct buffer *buf, int32_t x)
{
    int32_t be32 = htobe32(x);
    buf_append(buf, (char *)&be32, sizeof(int32_t));
}

void buf_appendInt16(struct buffer *buf, int16_t x)
{
    int16_t be16 = htobe16(x);
    buf_append(buf, (char *)&be16, sizeof(int16_t));
}

void buf_appendInt8(struct buffer *buf, int8_t x)
{
    buf_append(buf, (char *)&x, sizeof(int8_t));
}

void buf_prependInt64(struct buffer *buf, int64_t x)
{
    int64_t be64 = htobe64(x);
    buf_prepend(buf, (char *)&be64, sizeof(int64_t));
}

void buf_prependInt32(struct buffer *buf, int32_t x)
{
    int32_t be32 = htobe32(x);
    buf_prepend(buf, (char *)&be32, sizeof(int32_t));
}

void buf_prependInt16(struct buffer *buf, int16_t x)
{
    int16_t be16 = htobe16(x);
    buf_prepend(buf, (char *)&be16, sizeof(int16_t));
}

void buf_prependInt8(struct buffer *buf, int8_t x)
{
    buf_prepend(buf, (char *)&x, sizeof(int8_t));
}

int64_t buf_peekInt64(const struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int64_t));
    int64_t be64 = 0;
    memcpy(&be64, buf_peek(buf), sizeof(int64_t));
    return be64toh(be64);
}

int32_t buf_peekInt32(const struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int32_t));
    int32_t be32 = 0;
    memcpy(&be32, buf_peek(buf), sizeof(int32_t));
    return be32toh(be32);
}

int16_t buf_peekInt16(const struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int16_t));
    int16_t be16 = 0;
    memcpy(&be16, buf_peek(buf), sizeof(int16_t));
    return be16toh(be16);
}

int8_t buf_peekInt8(const struct buffer *buf)
{
    assert(buf_readable(buf) >= sizeof(int8_t));
    return *buf_peek(buf);
}

int64_t buf_readInt64(struct buffer *buf)
{
    int64_t x = buf_peekInt64(buf);
    buf_retrieveInt64(buf);
    return x;
}

int32_t buf_readInt32(struct buffer *buf)
{
    int32_t x = buf_peekInt32(buf);
    buf_retrieveInt32(buf);
    return x;
}

int16_t buf_readInt16(struct buffer *buf)
{
    int16_t x = buf_peekInt16(buf);
    buf_retrieveInt16(buf);
    return x;
}

int8_t buf_readInt8(struct buffer *buf)
{
    int8_t x = buf_peekInt8(buf);
    buf_retrieveInt8(buf);
    return x;
}

size_t buf_internalCapacity(struct buffer *buf)
{
    return buf->sz;
}

ssize_t buf_readFd(struct buffer *buf, int fd, int *errno_)
{
    char extrabuf[65535];
    struct iovec vec[2];
    size_t writable = buf_writable(buf);
    vec[0].iov_base = buf_beginWrite(buf);
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    int iovcnt = writable < sizeof(extrabuf) ? 2 : 1;
    ssize_t n = readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *errno_ = n;
    }
    else if (n <= writable)
    {
        buf->write_idx += n;
    }
    else
    {
        buf->write_idx = buf->sz;
        buf_append(buf, (char *)(&extrabuf[0]), n - writable);
    }

    return n;
}