/* util/pickle_write.c —— 见 pickle_write.h
 *
 * 序列化 RPA 索引为 pickle（协议 2，与 CPython pickle.dumps(index, 2) 字节等价，游戏可读）。
 * 采用与真实 Ren'Py 完全一致的写法：
 *   PROTO2 EMPTY_DICT MARK
 *     <key>  EMPTY_LIST  LONG4(off) LONG4(len) LONG4(prefix) TUPLE3  APPEND
 *     ...（每个文件重复）
 *   SETITEMS STOP
 * 注意：MARK 哨兵留在栈底，SETITEMS 取 MARK 之下的 dict 填充所有 (key,list) 对。
 */
#include "util/pickle_write.h"
#include "core/plugin.h"
#include "util/io_buf.h"
#include <stdlib.h>
#include <string.h>

static void put_u8(byte_buf *b, uint8_t v) { bb_append(b, &v, 1); }
static void put_u32le(byte_buf *b, uint32_t v) {
    uint8_t t[4] = { v & 0xff, (v>>8)&0xff, (v>>16)&0xff, (v>>24)&0xff };
    bb_append(b, t, 4);
}
/* LONG4：4字节小端长度 + 小端无符号整数（最多 8 字节） */
static void put_long4(byte_buf *b, uint64_t x) {
    uint8_t tmp[8]; int n = 0; uint64_t v = x;
    do { tmp[n++] = v & 0xff; v >>= 8; } while (v && n < 8);
    put_u8(b, 0x8b);
    put_u32le(b, (uint32_t)n);
    bb_append(b, tmp, n);
}

int pickle_write_rpa_index(const rpa_file_out_t *files, size_t nfiles,
                           uint8_t **out, size_t *outlen) {
    byte_buf b; bb_init(&b);
    put_u8(&b, 0x80); put_u8(&b, 0x02);   /* PROTO 2 */
    put_u8(&b, 0x7d);                      /* EMPTY_DICT */
    put_u8(&b, 0x28);                      /* MARK */

    for (size_t i = 0; i < nfiles; i++) {
        const rpa_file_out_t *f = &files[i];
        /* key: SHORT_BINUNICODE (1字节长度 + utf8) */
        size_t kl = strlen(f->name);
        if (kl > 255) kl = 255;
        put_u8(&b, 0x8c); put_u8(&b, (uint8_t)kl);
        bb_append(&b, f->name, kl);

        put_u8(&b, 0x5d);                  /* EMPTY_LIST */
        for (size_t j = 0; j < f->nblocks; j++) {
            put_long4(&b, f->blocks[j].offset);
            put_long4(&b, f->blocks[j].length);
            put_long4(&b, f->blocks[j].prefix);
            put_u8(&b, 0x87);              /* TUPLE3 */
        }
        put_u8(&b, 0x61);                  /* APPEND：tuple -> list */
    }
    put_u8(&b, 0x75);                      /* SETITEMS */
    put_u8(&b, 0x2e);                      /* STOP */

    *out = b.data;
    *outlen = b.len;
    return RP_OK;
}
