#ifndef DUBBO_HESSIAN_H
#define DUBBO_HESSIAN_H

#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>

char *utf82ascii(char *s);
size_t utf8len(const char *s, size_t sz);
int utf8cpy(uint8_t *dst, const uint8_t *src, size_t sz);

int hs_encode_null(uint8_t *out);
bool hs_decode_null(const uint8_t *buf, size_t sz);

int hs_encode_int(int32_t val, uint8_t *out);
bool hs_decode_int(const uint8_t *buf, size_t sz, int32_t *out);

int hs_encode_string(const char *str, uint8_t *out);
bool hs_decode_string(const uint8_t *buf, size_t sz, char **out, size_t *out_sz);

#endif