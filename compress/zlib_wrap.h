/* compress/zlib_wrap.h —— zlib 解压/压缩封装 */
#ifndef RENPY_PATCHER_ZLIB_WRAP_H
#define RENPY_PATCHER_ZLIB_WRAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 解压：in[inlen] -> *out（堆分配，调用方 free）。
 * 成功返回 RP_OK。 */
int zlib_inflate(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen);

/* 压缩：in[inlen] -> *out（堆分配，调用方 free）。level 取 zlib 的 0~9，或 -1 走默认。
 * 成功返回 RP_OK。 */
int zlib_deflate(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen, int level);

#ifdef __cplusplus
}
#endif
#endif
