/* util/pickle_min.h —— 最小 pickle 解析器（协议 2/3/4，足够解析 RPA 索引） */
#ifndef RENPY_PATCHER_PICKLE_MIN_H
#define RENPY_PATCHER_PICKLE_MIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PV_NONE = 0,
    PV_INT,
    PV_STR,
    PV_LIST,
    PV_TUPLE,
    PV_DICT
} pv_type;

typedef struct pv_value {
    pv_type type;
    uint64_t u;                          /* INT */
    char    *s;                          /* STR（UTF-8） */
    struct pv_value **items; size_t n;    /* LIST / TUPLE */
    struct pv_value **keys;              /* DICT */
    struct pv_value **vals;
    size_t nk;
} pv_value;

/* 解析 pickle 字节流（协议 2/3/4）。
 * 支持 RPA 索引用到的：PROTO/MARK/EMPTY_DICT/EMPTY_LIST/EMPTY_TUPLE/
 * LIST/TUPLE/TUPLE1/2/3/APPEND/APPENDS/SETITEM/SETITEMS/STOP，以及
 * 字符串（SHORT_BINUNICODE/BINUNICODE/SHORT_BINSTRING/BINSTRING/BINBYTES）
 * 与整数（BININT/BININT1/BININT2/BININT4/LONG1/LONG4）。
 * 成功返回 RP_OK，*out 为根节点（pv_free 释放）。 */
int pv_loads(const uint8_t *buf, size_t len, pv_value **out);

void pv_free(pv_value *v);

/* 便捷：取 dict 中键为 name（C 字符串）的值 */
const pv_value *pv_dict_get(const pv_value *dict, const char *name);

#ifdef __cplusplus
}
#endif
#endif
