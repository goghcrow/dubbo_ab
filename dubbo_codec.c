#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "endian.h"
#include "buffer.h"
#include "log.h"
#include "lib/cJSON.h"

#include "dubbo_codec.h"
#include "dubbo_hessian.h"

#define DUBBO_BUF_LEN 8192
#define DUBBO_MAX_PKT_SZ (1024 * 1024 * 4)
#define DUBBO_HDR_LEN 16
#define DUBBO_MAGIC 0xdabb
#define DUBBO_VER "3.1.0-RELEASE"

/*
public interface GenericService {
    **
     * 泛化调用，方法参数和返回结果使用json序列化，方法参数的key从arg0开始
     * @param method
     * @param parameterTypes
     * @param jsonArgs
     * @return
     * @throws GenericException
     *
    String $invokeWithJsonArgs(String method, String[] parameterTypes, String jsonArgs) throws GenericException;
}
*/
#define DUBBO_GENERIC_METHOD_NAME "$invokeWithJsonArgs"
#define DUBBO_GENERIC_METHOD_VER "0.0.0"
#ifdef DUBBO_BYTE_CODEC
#define DUBBO_GENERIC_METHOD_PARA_TYPES "Ljava/lang/String;[Ljava/lang/String;[B;"
#else
#define DUBBO_GENERIC_METHOD_PARA_TYPES "Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;"
#endif
#define DUBBO_GENERIC_METHOD_ARGC 3
#define DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX 0
#define DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX 1
#define DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX 2

#define DUBBO_HESSIAN2_SERI_ID 2

#define DUBBO_FLAG_REQ 0x80
#define DUBBO_FLAG_TWOWAY 0x40
#define DUBBO_FLAG_EVT 0x20

#define DUBBO_SERI_MASK 0x1f

#define DUBBO_RES_T_OK 20
#define DUBBO_RES_T_CLIENT_TIMEOUT 30
#define DUBBO_RES_T_SERVER_TIMEOUT 31
#define DUBBO_RES_T_BAD_REQUEST 40
#define DUBBO_RES_T_BAD_RESPONSE 50
#define DUBBO_RES_T_SERVICE_NOT_FOUND 60
#define DUBBO_RES_T_SERVICE_ERROR 70
#define DUBBO_RES_T_SERVER_ERROR 80
#define DUBBO_RES_T_CLIENT_ERROR 90

struct dubbo_req
{
    int64_t reqid;
    bool is_twoway;
    bool is_evt;

    char *service; // java string -> hessian string
    char *method;  // java string -> hessian string
    char **argv;   // java string[] -> hessian string[]
    int argc;
    char *attach; // java map<string, string> -> hessian map<string, string>

    // for evt
    char *data;
    size_t data_sz;
};

struct dubbo_hdr
{
    int8_t flag;
    int8_t status;
    int32_t body_sz;
    int64_t reqid;
};

static char *get_res_status_desc(int8_t status)
{
    switch (status)
    {
    case DUBBO_RES_T_OK:
        return "OK";
    case DUBBO_RES_T_CLIENT_TIMEOUT:
        return "CLIENT TIMEOUT";
    case DUBBO_RES_T_SERVER_TIMEOUT:
        return "SERVER TIMEOUT";
    case DUBBO_RES_T_BAD_REQUEST:
        return "BAD REQUEST";
    case DUBBO_RES_T_BAD_RESPONSE:
        return "BAD RESPONSE";
    case DUBBO_RES_T_SERVICE_NOT_FOUND:
        return "SERVICE NOT FOUND";
    case DUBBO_RES_T_SERVICE_ERROR:
        return "SERVICE ERROR";
    case DUBBO_RES_T_SERVER_ERROR:
        return "SERVER ERROR";
    case DUBBO_RES_T_CLIENT_ERROR:
        return "CLIENT ERROR";
    default:
        return "UNKNOWN";
    }
}

static int64_t next_reqid()
{
    static int64_t id = 0;
    if (++id == 0x7fffffffffffffff)
    {
        id = 1;
    }
    return id;
}

static char *rebuild_json_args(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        return NULL;
    }
    if (!cJSON_IsArray(root) && !cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *arr = cJSON_CreateArray();
    cJSON *el;
    cJSON_ArrayForEach(el, root)
    {
        cJSON_AddItemToArray(arr, cJSON_Duplicate(el, true));
    }
    cJSON_Delete(root);

    // fixme 消除内存 copy
    char *utf8_json = cJSON_PrintUnformatted(arr);
    char *ascii_s = utf82ascii(utf8_json);
    free(utf8_json);
    return ascii_s;
}

// fixme
// static char *rebuild_json_attach(char *json_str)
// {
//     return NULL;
// }

static bool encode_req_hdr(struct buffer *buf, const struct dubbo_hdr *hdr)
{
    if (buf_prependable(buf) < DUBBO_HDR_LEN)
    {
        return false;
    }

    buf_prependInt32(buf, hdr->body_sz);
    buf_prependInt64(buf, hdr->reqid);
    buf_prependInt8(buf, hdr->status);
    buf_prependInt8(buf, hdr->flag);
    buf_prependInt16(buf, DUBBO_MAGIC); // fixme
    return true;
}

static bool decode_res_hdr(struct buffer *buf, struct dubbo_hdr *hdr)
{
    if (!is_dubbo_pkt(buf))
    {
        return false;
    }

    buf_readInt16(buf); // magic
    hdr->flag = buf_readInt8(buf);
    hdr->status = buf_readInt8(buf);
    hdr->reqid = buf_readInt64(buf);
    hdr->body_sz = buf_readInt32(buf);

    if (hdr->body_sz > DUBBO_MAX_PKT_SZ)
    {
        LOG_ERROR("too big dubbo pkt body size: %d", hdr->body_sz);
        return false;
    }

    int8_t seria_id = hdr->flag & DUBBO_SERI_MASK;
    if (seria_id != DUBBO_HESSIAN2_SERI_ID)
    {
        LOG_ERROR("unsupport dubbo serialization id %d", seria_id);
        return false;
    }

    if ((hdr->flag & DUBBO_FLAG_REQ) == 0)
    {
        // decode response
        return true;
    }
    else
    {
        // decode request
        LOG_ERROR("unsupport decode dubbo request packet");
        return false;
    }
}

#define write_hs_str(buf, s)                                             \
    {                                                                    \
        int n = hs_encode_string((s), (uint8_t *)buf_beginWrite((buf))); \
        if (n == -1)                                                     \
        {                                                                \
            return false;                                                \
        }                                                                \
        buf_has_written(buf, n);                                         \
    }

static bool encode_req_data(struct buffer *buf, const struct dubbo_req *req)
{
    write_hs_str(buf, DUBBO_VER);
    write_hs_str(buf, req->service);
    write_hs_str(buf, DUBBO_GENERIC_METHOD_VER);
    write_hs_str(buf, DUBBO_GENERIC_METHOD_NAME);
    write_hs_str(buf, DUBBO_GENERIC_METHOD_PARA_TYPES);

    // args
    write_hs_str(buf, req->argv[DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX]);
    buf_has_written(buf, hs_encode_null((uint8_t *)buf_beginWrite(buf))); // 方法类型提示 NULL, 不支持重载方法
#ifdef DUBBO_BYTE_CODEC
    const char *args = req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX];
    hs_encode_binary(args, strlen(args), buf);
#else
    write_hs_str(buf, req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX]);
#endif

    // TODO: fixme :  attach NULL
    buf_has_written(buf, hs_encode_null((uint8_t *)buf_beginWrite(buf)));

    return true;
}

#define read_hs_str(buf, out, out_sz)                                                        \
    if (!hs_decode_string((uint8_t *)buf_peek((buf)), buf_readable((buf)), (out), (out_sz))) \
    {                                                                                        \
        LOG_ERROR("failed to decode hessian string");                                        \
        return false;                                                                        \
    }                                                                                        \
    buf_retrieve(buf, *(out_sz));

static bool decode_res_data(struct buffer *buf, const struct dubbo_hdr *hdr, struct dubbo_res *res)
{
    uint8_t flag = buf_readInt8(buf);
    // hessian2 小整数
    if (flag < 0x80 || flag > 0xbf)
    {
        LOG_ERROR("invalid response type %d (raw)", flag);
        return false;
    }
    flag -= 0x90;

    switch (flag)
    {
    case DUBBO_RES_NULL:
        break;
    case DUBBO_RES_EX:
        read_hs_str(buf, &res->data, &res->data_sz);
        break;
    case DUBBO_RES_VAL:
#ifdef DUBBO_BYTE_CODEC
        hs_decode_binary(buf, &res->data, &res->data_sz);
#else
        read_hs_str(buf, &res->data, &res->data_sz);
#endif
        break;
    default:
        LOG_ERROR("unknown result flag, expect '0' '1' '2', get %d", res->type);
        return false;
    }

    res->type = flag;
    return true;
}

void dubbo_res_release(struct dubbo_res *res)
{
    if (res->desc)
    {
        free(res->desc);
    }
    if (res->data)
    {
        free(res->data);
    }
    if (res->attach)
    {
        free(res->attach);
    }
    free(res);
}

static bool encode_req(struct buffer *buf, const struct dubbo_req *req)
{
    struct dubbo_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.reqid = req->reqid;
    hdr.status = 0;
    hdr.flag = (int8_t)DUBBO_FLAG_REQ | (int8_t)DUBBO_HESSIAN2_SERI_ID;
    if (req->is_twoway)
    {
        hdr.flag |= DUBBO_FLAG_TWOWAY;
    }
    if (req->is_evt)
    {
        hdr.flag |= DUBBO_FLAG_EVT;
    }

    if (req->is_evt)
    {
        buf_append(buf, req->data, req->data_sz);
    }
    else
    {
        if (!encode_req_data(buf, req))
        {
            LOG_ERROR("failed to encode req data");
            return false;
        }
    }
    hdr.body_sz = buf_readable(buf);

    if (!encode_req_hdr(buf, &hdr))
    {
        LOG_ERROR("failed to encode req hdr");
        return false;
    }

    return true;
}

static bool decode_res(struct buffer *buf, const struct dubbo_hdr *hdr, struct dubbo_res *res)
{
    res->is_evt = hdr->flag & DUBBO_FLAG_EVT;
    res->desc = strdup(get_res_status_desc(hdr->status));

    if (hdr->status == DUBBO_RES_T_OK)
    {
        res->ok = true;
        if (res->is_evt)
        {
            // decode evt
        }
        else
        {
            if (!decode_res_data(buf, hdr, res))
            {
                LOG_ERROR("failed to decode response data");
                return false;
            }
        }
    }
    else
    {
        res->ok = false;
        read_hs_str(buf, &res->data, &res->data_sz);
    }

    // fixme read attach

    return true;
}

struct dubbo_req *dubbo_req_create(const char *service, const char *method, const char *json_args, const char *json_attach)
{
    struct dubbo_req *req = calloc(1, sizeof(*req));
    assert(req);
    req->reqid = next_reqid();
    req->is_twoway = true;
    req->is_evt = false;
    req->service = strdup(service);
    req->method = strdup(method);

    char *args = rebuild_json_args(json_args);
    if (args == NULL)
    {
        LOG_ERROR("invalid json args %s", json_args);
        return NULL;
    }

    req->argc = DUBBO_GENERIC_METHOD_ARGC;
    req->argv = calloc(3, sizeof(void *));
    assert(req->argv);
    req->argv[DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX] = strdup(method);
    req->argv[DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX] = NULL;
    req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX] = args;

    // fixme 要处理成 hessian map
    if (json_attach)
    {
        req->attach = strdup(json_attach);
    }

    return req;
}

void dubbo_req_release(struct dubbo_req *req)
{
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_METHOD_IDX]);
    // free(req->argv[DUBBO_GENERIC_METHOD_ARGV_TYPES_IDX]);
    free(req->argv[DUBBO_GENERIC_METHOD_ARGV_ARGS_IDX]);
    free(req->argv);
    if (req->attach)
    {
        free(req->attach);
    }
    free(req);
}

int64_t dubbo_req_getid(struct dubbo_req *req)
{
    return req->reqid;
}

struct buffer *dubbo_encode(const struct dubbo_req *req)
{
    struct buffer *buf = buf_create_ex(DUBBO_BUF_LEN, DUBBO_HDR_LEN);
    if (encode_req(buf, req))
    {
        return buf;
    }
    else
    {
        buf_release(buf);
        return NULL;
    }
}

struct dubbo_res *dubbo_decode(struct buffer *buf)
{
    struct dubbo_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    if (!decode_res_hdr(buf, &hdr))
    {
        LOG_ERROR("failed to decode dubbo response");
        return NULL;
    }

    // 读取所有 body+attach
    // struct buffer *body_buf = buf_create(hdr.body_sz);
    // buf_append(body_buf, buf_peek(buf), hdr.body_sz);

    struct buffer *body_buf = buf_readonlyView(buf, hdr.body_sz);
    buf_retrieve(buf, hdr.body_sz);

    struct dubbo_res *res = calloc(1, sizeof(*res));
    assert(res);
    res->type = -1;
    res->reqid = hdr.reqid;

    bool ok = decode_res(body_buf, &hdr, res);
    buf_release(body_buf);

    if (ok)
    {
        return res;
    }
    else
    {
        free(res);
        return NULL;
    }
}

bool is_dubbo_pkt(const struct buffer *buf)
{
    return buf_readable(buf) >= DUBBO_HDR_LEN && (uint16_t)buf_peekInt16(buf) == DUBBO_MAGIC;
}

// remaining 0: completed,  > 0, not completed
bool is_completed_dubbo_pkt(const struct buffer *buf, int *remaining)
{
    if (!is_dubbo_pkt(buf))
    {
        return false;
    }

    int32_t body_sz = 0;
    memcpy(&body_sz, buf_peek(buf) + DUBBO_HDR_LEN - sizeof(int32_t), sizeof(int32_t));
    body_sz = be32toh(body_sz);
    if (body_sz <= 0 || body_sz > DUBBO_MAX_PKT_SZ)
    {
        LOG_ERROR("invalid dubbo pkt body size %d", body_sz);
        return false;
    }

    if (remaining)
    {
        *remaining = body_sz + DUBBO_HDR_LEN - buf_readable(buf);
    }
    return true;
}