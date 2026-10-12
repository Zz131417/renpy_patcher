/* util/io_buf.h —— 通用 I/O 缓冲：整个文件读入内存 + 可增长字节缓冲 */
#ifndef RENPY_PATCHER_IO_BUF_H
#define RENPY_PATCHER_IO_BUF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 把整个文件读入 *out（堆分配，调用方 free）。返回 RP_OK。 */
int read_file_all(const char *path, uint8_t **out, size_t *outlen);

/* 可增长字节缓冲 */
typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} byte_buf;

void bb_init(byte_buf *b);
/* 追加数据；返回 RP_OK / RP_ERR_NOMEM */
int  bb_append(byte_buf *b, const void *p, size_t n);
void bb_free(byte_buf *b);

#ifdef __cplusplus
}
#endif
#endif
