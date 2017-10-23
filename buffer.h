#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stddef.h>    /*size_t*/
#include <sys/types.h> /*ssize_t*/

#define BufCheapPrepend 8

struct buffer;

#define buf_create(s) buf_create_ex((s), BufCheapPrepend)

struct buffer *buf_create_ex(size_t size, size_t prepend_size);
void buf_release(struct buffer *buf);
size_t buf_getReadIndex(struct buffer *buf);
void buf_setReadIndex(struct buffer *buf, size_t read_idx);
size_t buf_getWriteIndex(struct buffer *buf);
void buf_setWriteIndex(struct buffer *buf, size_t write_idx);
size_t buf_readable(const struct buffer *buf);
size_t buf_writable(const struct buffer *buf);
size_t buf_prependable(const struct buffer *buf);
const char *buf_peek(const struct buffer *buf);
char *buf_beginWrite(struct buffer *buf);
void buf_has_written(struct buffer *buf, size_t len);
void buf_unwrite(struct buffer *buf, size_t len);
const char *buf_findCRLF(struct buffer *buf);
const char *buf_findEOL(struct buffer *buf);
void buf_retrieveAsString(struct buffer *buf, size_t len, char *str);
void buf_retrieveAll(struct buffer *buf);
void buf_retrieve(struct buffer *buf, size_t len);
void buf_retrieveUntil(struct buffer *buf, const char *end);
void buf_retrieveInt64(struct buffer *buf);
void buf_retrieveInt32(struct buffer *buf);
void buf_retrieveInt16(struct buffer *buf);
void buf_retrieveInt8(struct buffer *buf);
void buf_ensureWritable(struct buffer *buf, size_t len);
void buf_append(struct buffer *buf, const char *data, size_t len);
void buf_prepend(struct buffer *buf, const char *data, size_t len);
void buf_shrink(struct buffer *buf, size_t reserve);
void buf_appendInt64(struct buffer *buf, int64_t x);
void buf_appendInt32(struct buffer *buf, int32_t x);
void buf_appendInt16(struct buffer *buf, int16_t x);
void buf_appendInt8(struct buffer *buf, int8_t x);
void buf_prependInt64(struct buffer *buf, int64_t x);
void buf_prependInt32(struct buffer *buf, int32_t x);
void buf_prependInt16(struct buffer *buf, int16_t x);
void buf_prependInt8(struct buffer *buf, int8_t x);
int64_t buf_peekInt64(const struct buffer *buf);
int32_t buf_peekInt32(const struct buffer *buf);
int16_t buf_peekInt16(const struct buffer *buf);
int8_t buf_peekInt8(const struct buffer *buf);
int64_t buf_readInt64(struct buffer *buf);
int32_t buf_readInt32(struct buffer *buf);
int16_t buf_readInt16(struct buffer *buf);
int8_t buf_readInt8(struct buffer *buf);
size_t buf_internalCapacity(struct buffer *buf);
ssize_t buf_readFd(struct buffer *buf, int fd, int *errno_);

#endif