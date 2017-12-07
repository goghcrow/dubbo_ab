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

// 一定要看这个链接的文档, 小心其他文档 !!!
// http://hessian.caucho.com/doc/hessian-serialization.html
/*
top        ::= value

           # 8-bit binary data split into 64k chunks
binary     ::= x41 b1 b0 <binary-data> binary # non-final chunk
           ::= 'B' b1 b0 <binary-data>        # final chunk
           ::= [x20-x2f] <binary-data>        # binary data of
                                                 #  length 0-15
           ::= [x34-x37] <binary-data>        # binary data of
                                                 #  length 0-1023

           # boolean true/false
boolean    ::= 'T'
           ::= 'F'

           # definition for an object (compact map)
class-def  ::= 'C' string int string*

           # time in UTC encoded as 64-bit long milliseconds since
           #  epoch
date       ::= x4a b7 b6 b5 b4 b3 b2 b1 b0
           ::= x4b b3 b2 b1 b0       # minutes since epoch

           # 64-bit IEEE double
double     ::= 'D' b7 b6 b5 b4 b3 b2 b1 b0
           ::= x5b                   # 0.0
           ::= x5c                   # 1.0
           ::= x5d b0                # byte cast to double
                                     #  (-128.0 to 127.0)
           ::= x5e b1 b0             # short cast to double
           ::= x5f b3 b2 b1 b0       # 32-bit float cast to double

           # 32-bit signed integer
int        ::= 'I' b3 b2 b1 b0
           ::= [x80-xbf]             # -x10 to x3f
           ::= [xc0-xcf] b0          # -x800 to x7ff
           ::= [xd0-xd7] b1 b0       # -x40000 to x3ffff

           # list/vector
list       ::= x55 type value* 'Z'   # variable-length list
    ::= 'V' type int value*   # fixed-length list
           ::= x57 value* 'Z'        # variable-length untyped list
           ::= x58 int value*        # fixed-length untyped list
    ::= [x70-77] type value*  # fixed-length typed list
    ::= [x78-7f] value*       # fixed-length untyped list

           # 64-bit signed long integer
long       ::= 'L' b7 b6 b5 b4 b3 b2 b1 b0
           ::= [xd8-xef]             # -x08 to x0f
           ::= [xf0-xff] b0          # -x800 to x7ff
           ::= [x38-x3f] b1 b0       # -x40000 to x3ffff
           ::= x59 b3 b2 b1 b0       # 32-bit integer cast to long

           # map/object
map        ::= 'M' type (value value)* 'Z'  # key, value map pairs
    ::= 'H' (value value)* 'Z'       # untyped key, value

           # null value
null       ::= 'N'

           # Object instance
object     ::= 'O' int value*
    ::= [x60-x6f] value*

           # value reference (e.g. circular trees and graphs)
ref        ::= x51 int            # reference to nth map/list/object

           # UTF-8 encoded character string split into 64k chunks
string     ::= x52 b1 b0 <utf8-data> string  # non-final chunk
           ::= 'S' b1 b0 <utf8-data>         # string of length
                                             #  0-65535
           ::= [x00-x1f] <utf8-data>         # string of length
                                             #  0-31
           ::= [x30-x34] <utf8-data>         # string of length
                                             #  0-1023

           # map/list types for OO languages
type       ::= string                        # type name
           ::= int                           # type reference

           # main production
value      ::= null
           ::= binary
           ::= boolean
           ::= class-def value
           ::= date
           ::= double
           ::= int
           ::= list
           ::= long
           ::= map
           ::= object
           ::= ref
           ::= string
*/

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

/* 64k */
#define BIN_CHUNK_MAX 0x10000

void hs_encode_binary(const char *bin, size_t sz, struct buffer *out_buf)
{
    // 不做判断, 最后一块体积填0 ?!
    // int just_right = sz % chunk_max == 0;
    while (sz > BIN_CHUNK_MAX)
    {
        buf_appendInt8(out_buf, 'b');
        buf_appendInt16(out_buf, (uint16_t)BIN_CHUNK_MAX);
        buf_append(out_buf, bin, BIN_CHUNK_MAX);
        bin += BIN_CHUNK_MAX;
        sz -= BIN_CHUNK_MAX;
    }
    buf_appendInt8(out_buf, 'B');
    buf_appendInt16(out_buf, sz);
    if (sz)
    {
        buf_append(out_buf, bin, sz);
    }
}

static bool hs_decode_binary_chunk(struct buffer *buf, char **out, size_t *out_sz, size_t *left)
{
    int sz = buf_readInt16(buf);
    if (*left < sz)
    {
        char *new_out = realloc(*out, *out_sz + BIN_CHUNK_MAX + 1);
        if (new_out == NULL)
        {
            return false;
        }
        *out = new_out;
        *left = BIN_CHUNK_MAX;
    }

    memcpy(*out, buf_peek(buf), sz);
    *out_sz += sz;
    *left -= sz;
    buf_retrieve(buf, sz);
    return true;
}

/*
           # 8-bit binary data split into 64k chunks
binary     ::= x41 b1 b0 <binary-data> binary # non-final chunk
           ::= 'B' b1 b0 <binary-data>        # final chunk
           ::= [x20-x2f] <binary-data>        # binary data of
                                                 #  length 0-15
           ::= [x34-x37] <binary-data>        # binary data of
                                                 #  length 0-1023
*/
bool hs_decode_binary(struct buffer *buf, char **out, size_t *out_sz)
{
    uint16_t tag = buf_peekInt8(buf);
    uint16_t sz = 0;
    if (tag >= 0x20 && tag <= 0x2f)
    {
        sz = tag - 0x20;
        
        small:
        buf_retrieveInt8(buf);
        *out_sz = sz;
        *out = malloc(sz + 1);
        if (*out == NULL)
        {
            return false;
        }
        memcpy(*out, buf_peek(buf), sz);
        buf_retrieve(buf, sz);
        return true;
    }
    else if (tag >= 0x34 && tag <= 0x37)
    {
        uint16_t sz = buf_readInt16(buf);
        sz = sz & 1023; // 10 bit number !!!
        goto small;
    }
    else
    {
        buf_retrieveInt8(buf);

        size_t left = BIN_CHUNK_MAX;
        *out_sz = 0;
        *out = malloc(BIN_CHUNK_MAX + 1);
        if (*out == NULL)
        {
            return false;
        }

        while (tag == 0x41)
        {
            if (!hs_decode_binary_chunk(buf, out, out_sz, &left))
            {
                free(*out);
                *out = NULL;
                return false;
            }
            tag = buf_readInt8(buf);
        }

        assert(tag == 'B');
        if (!hs_decode_binary_chunk(buf, out, out_sz, &left))
        {
            free(*out);
            *out = NULL;
            return false;
        }
    }
    *out[*out_sz] = 0;
    return true;
}