#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dubbo_hessian.h"
#include "endian.h"
#include "buffer.h"
#include "lib/utf8_decode.h"

static const char digits[] = "0123456789abcdef";

// 非法 utf8 返回 null, 正常返回 null 结尾 char*
char *utf82ascii(char *s)
{
    struct buffer *buf = buf_create(strlen(s) * 2);

    utf8_decode_init(s, strlen(s));
    int c = utf8_decode_next();
    while (c != UTF8_END)
    {
        if (c == UTF8_ERROR)
        {
            buf_release(buf);
            return NULL;
        }

        if (c >= 0 && c <= 127)
        {
            buf_appendInt8(buf, c);
        }
        else
        {
            /* From http://en.wikipedia.org/wiki/UTF16 */
            if (c >= 0x10000)
            {
                unsigned int next_c;
                c -= 0x10000;
                next_c = (unsigned short)((c & 0x3ff) | 0xdc00);
                c = (unsigned short)((c >> 10) | 0xd800);

                buf_append(buf, "\\u", 2);
                buf_appendInt8(buf, digits[(c & 0xf000) >> 12]);
                buf_appendInt8(buf, digits[(c & 0xf00) >> 8]);
                buf_appendInt8(buf, digits[(c & 0xf0) >> 4]);
                buf_appendInt8(buf, digits[(c & 0xf)]);
                c = next_c;
            }

            buf_append(buf, "\\u", 2);
            buf_appendInt8(buf, digits[(c & 0xf000) >> 12]);
            buf_appendInt8(buf, digits[(c & 0xf00) >> 8]);
            buf_appendInt8(buf, digits[(c & 0xf0) >> 4]);
            buf_appendInt8(buf, digits[(c & 0xf)]);
        }
        c = utf8_decode_next();
    }

    char *ret = malloc(buf_readable(buf) + 1);
    assert(ret);
    buf_retrieveAsString(buf, buf_readable(buf), ret);
    buf_release(buf);
    return ret;
}

size_t utf8len(const char *s, size_t sz)
{
    size_t len = 0;
    size_t i = 0;
    // for (; *s; ++s)
    for (; i < sz; i++)
    {
        if ((*s & 0xC0) != 0x80)
        {
            ++len;
        }
    }
    return len;
}

// 复制 size 个 utf8 字符, 返回实际复制字节数, 遇到非法 utf8 字符返回 -1
int utf8cpy(uint8_t *dst, const uint8_t *src, size_t sz)
{
    int i = 0;
    uint8_t c;

    while (sz--)
    {
        c = src[i];
        if (c <= 0x80)
        {
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            i += 4;
        }
        else
        {
            // invalid utf8
            return -1;
        }
    }

    if (i)
    {
        memcpy(dst, src, i);
    }

    return i;
}

// http://hessian.caucho.com/doc/hessian-serialization.html

int hs_encode_null(uint8_t *out)
{
    out[0] = 'N';
    return 1;
}

bool hs_decode_null(const uint8_t *buf, size_t sz)
{
    return sz == 1 && buf[0] == 'N';
}

int hs_encode_int(int32_t val, uint8_t *out)
{
    if (-0x10 <= val && val <= 0x2f)
    {
        out[0] = val + 0x90;
        return 1;
    }
    else if (-0x800 <= val + 0xc8 && val <= 0x7ff)
    {
        out[1] = val & 0xff;
        out[0] = (val >> 8) + 0xc8;
        return 2;
    }
    else if (-0x40000 <= val && val <= 0x3ffff)
    {
        out[0] = (val >> 16) + 0xd4;
        *(uint16_t *)(out + 1) = htobe16(val & 0xffff);
        return 3;
    }
    else
    {
        out[0] = 'I';
        *(int32_t *)(out + 1) = htobe32(val);
        return 5;
    }
    return -1;
}

bool hs_decode_int(const uint8_t *buf, size_t sz, int32_t *out)
{
    bool r = true;
    uint8_t code = buf[0];
    if (sz >= 1 && code >= 0x80 && code <= 0xbf)
    {
        *out = code - 0x90;
    }
    else if (sz >= 2 && code >= 0xc0 && code <= 0xcf)
    {
        *out = ((code - 0xc8) << 8) + buf[1];
    }
    else if (sz >= 3 && code >= 0xd0 && code <= 0xd7)
    {
        *out = ((code - 0xd4) << 16) + (buf[1] << 8) + buf[2];
    }
    else if (sz >= 5 && code == 'I')
    {
        *out = be32toh(*(uint32_t *)&buf[1]);
    }
    else
    {
        r = false;
    }
    return r;
}

// !!!!! 注意: 这里读出来的长度是指 UTF8 字符长度, 而非字节长度
// out_length 修改为返回 字节数, 而非字符数
static bool internal_decode_string(const uint8_t *buf, size_t buf_length, uint8_t *out_str, size_t *out_length, short *is_last_chunk)
{
    uint8_t code = buf[0];
    size_t delta_length;
    short result;
    int sz;

    switch (code)
    {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x0e:
    case 0x0f:

    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f:
        *is_last_chunk = 1;
        delta_length = code - 0x00;
        if (buf_length < 1 + delta_length)
        {
            return false;
        }
        sz = utf8cpy(out_str + *out_length, buf + 1, delta_length);
        if (sz == -1)
        {
            return false;
        }
        *out_length = *out_length + sz;
        return true;

    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
        *is_last_chunk = 1;
        if (buf_length < 2)
        {
            return false;
        }
        delta_length = (code - 0x30) * 256 + buf[1];
        if (buf_length < 2 + delta_length)
        {
            return false;
        }
        sz = utf8cpy(out_str + *out_length, buf + 2, delta_length);
        if (sz == -1)
        {
            return false;
        }
        *out_length = *out_length + sz;
        return true;

    case 0x53:
        *is_last_chunk = 1;
        if (buf_length < 3)
        {
            return false;
        }
        delta_length = be16toh(*(uint16_t *)(buf + 1));
        if (buf_length < 3 + delta_length)
        {
            return false;
        }
        sz = utf8cpy(out_str + *out_length, buf + 3, delta_length);
        if (sz == -1)
        {
            return false;
        }
        *out_length = *out_length + sz;
        return true;

    case 0x52:
        *is_last_chunk = 0;
        if (buf_length < 3)
        {
            return false;
        }
        delta_length = be16toh(*(uint16_t *)(buf + 1));
        if (buf_length < 3 + delta_length)
        {
            return false;
        }
        sz = utf8cpy(out_str + *out_length, buf + 3, delta_length);
        if (sz == -1)
        {
            return false;
        }
        *out_length = *out_length + sz;
        while (!*is_last_chunk)
        {
            result = internal_decode_string(buf, buf_length, out_str, out_length, is_last_chunk);
            if (!result)
            {
                return false;
            }
        }
        break;
    }
    return false;
}

int hs_encode_string(const char *str, uint8_t *out)
{
    size_t index = 0;
    int length = strlen(str);

    // TODO
    if (length > 0x8000)
    {
        return -1;
    }

    if (length <= 31)
    {
        out[index++] = (uint8_t)(length);
    }
    else if (length <= 1023)
    {
        out[index++] = (uint8_t)(48 + (length >> 8));
        // Integer overflow and wrapping assumed
        out[index++] = (uint8_t)(length);
    }
    else
    {
        out[index++] = 'S';
        out[index++] = (uint8_t)((length >> 8));
        // Integer overflow and wrapping assumed
        out[index++] = (uint8_t)(length);
    }

    memcpy(out + index, str, length);
    return index + length;
}

// !! FREE
bool hs_decode_string(const uint8_t *buf, size_t sz, char **out, size_t *out_sz)
{
    short is_last_chunk = 0;
    size_t out_length = 0;
    uint8_t *out_str = (uint8_t *)malloc(sz);
    if (NULL == out_str)
    {
        return false;
    }

    if (internal_decode_string(buf, sz, out_str, &out_length, &is_last_chunk))
    {
        uint8_t *new_out = (uint8_t *)realloc(out_str, out_length);
        if (NULL != new_out)
        {
            out_str = new_out;
        }
        *out = (char *)out_str;
        *out_sz = out_length;
        return true;
    }
    else
    {
        free(out_str);
        return false;
    }
}