/* util/dir_walk.h —— 递归收集目录下所有文件 */
#ifndef RENPY_PATCHER_DIR_WALK_H
#define RENPY_PATCHER_DIR_WALK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     full_path[2048];   /* 绝对/基于调用点的完整路径 */
    char     rel_path[2048];    /* 相对根目录的路径，用 '/' 分隔 */
    size_t   size;
} file_entry_t;

/* 递归收集 dir 下所有普通文件，按 rel_path 升序排列。
 * *out 为堆分配数组（调用方 free_files 释放）。成功返回 RP_OK。 */
int  collect_files(const char *dir, file_entry_t **out, size_t *n);

void free_files(file_entry_t *files, size_t n);

#ifdef __cplusplus
}
#endif
#endif
