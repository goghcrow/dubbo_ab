#ifndef DUBBO_CODEC_H
#define DUBBO_CODEC_H

#include <stdbool.h>

#include "buffer.h"

// dubbo 泛化调用 codec

/* dubbo 泛化调用
public interface GenericService {
    **
     * 泛化调用 (默认实现, 不支持)
     *
     * @param method 方法名，如：findPerson，如果有重载方法，需带上参数列表，如：findPerson(java.lang.String)
     * @param parameterTypes 参数类型
     * @param args 参数列表
     * @return 返回值
     * @throws Throwable 方法抛出的异常
     *
    Object $invoke(String method, String[] parameterTypes, Object[] args) throws GenericException;

    **
     * (支持)
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

#define DUBBO_RES_EX 0
#define DUBBO_RES_VAL 1
#define DUBBO_RES_NULL 2

typedef enum {
    ex = DUBBO_RES_EX,
    val = DUBBO_RES_VAL,
    null = DUBBO_RES_NULL,
} dubbo_res_type;

struct dubbo_req;

struct dubbo_res
{
    int64_t reqid;
    bool is_evt;
    bool ok;
    dubbo_res_type type;
    char *desc;
    char *data;
    size_t data_sz;
    char *attach;
    size_t attach_sz;
};

struct dubbo_req *dubbo_req_create(const char *service, const char *method, const char *json_args, const char *json_attach);
void dubbo_req_release(struct dubbo_req *);
int64_t dubbo_req_getid(struct dubbo_req *);
void dubbo_res_release(struct dubbo_res *);

struct buffer *dubbo_encode(const struct dubbo_req *);
struct dubbo_res *dubbo_decode(struct buffer *);

bool is_dubbo_pkt(const struct buffer *);

// remaining   0: completed,  < 0, not completed, > 0 overflow
bool is_completed_dubbo_pkt(const struct buffer *buf, int *remaining);

#endif