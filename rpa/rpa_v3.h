/*
 * rpa/rpa_v3.h —— RPA 3.0 解包/打包 handler
 */
#ifndef RENPY_PATCHER_RPA_V3_H
#define RENPY_PATCHER_RPA_V3_H

#include "core/plugin.h"
#include "rpa/rpa_format.h"
#include "util/pickle_min.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint64_t offset; uint64_t length; uint32_t prefix; } rpa_block_t;
typedef struct { char *name; size_t nblocks; rpa_block_t *blocks; } rpa_file_t;
typedef struct { size_t nfiles; rpa_file_t *files; } rpa_index_t;
typedef struct { uint32_t prefix; uint32_t index_len; } rpa_header_t;

int rpa_v3_can_handle(const uint8_t *hdr, size_t len);
int rpa_v3_read_header(const uint8_t *buf, size_t len, rpa_header_t *out);
int rpa_index_from_pickle(const pv_value *root, rpa_index_t *out);
void rpa_index_free(rpa_index_t *idx);

int rpa_v3_unpack(const char *in_file, const char *out_dir);

/* 统计 rpa 内部文件数（用于游戏管理展示，不改变文件） */
int rpa_v3_stat(const char *path, size_t *nfiles);
int rpa_v3_repack(const char *dir, const char *out_file);

#ifdef __cplusplus
}
#endif
#endif
