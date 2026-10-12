/* util/pickle_write.h —— 最小 pickle 序列化器（用于写 RPA 索引） */
#ifndef RENPY_PATCHER_PICKLE_WRITE_H
#define RENPY_PATCHER_PICKLE_WRITE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpa_block_out {
    uint64_t offset;
    uint64_t length;
    uint32_t prefix;
} rpa_block_out_t;

typedef struct rpa_file_out {
    const char *name;          /* UTF-8，用 '/' 分隔 */
    size_t     nblocks;
    const rpa_block_out_t *blocks;
} rpa_file_out_t;

/*
 * 序列化 RPA 索引为 pickle（协议 2，与 CPython 字节等价，游戏可读）：
 *   PROTO2 EMPTY_DICT MARK
 *     key  EMPTY_LIST  (块: 3×LONG4 TUPLE3 APPEND)*
 *     ...
 *   SETITEMS STOP
 * 成功返回 RP_OK，*out 为堆分配的 pickle 字节（调用方 free）。
 */
int pickle_write_rpa_index(const rpa_file_out_t *files, size_t nfiles,
                           uint8_t **out, size_t *outlen);

#ifdef __cplusplus
}
#endif
#endif
