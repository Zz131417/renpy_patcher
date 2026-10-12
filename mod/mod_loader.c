/* mod/mod_loader.c —— 见 mod/mod_manifest.h */
#include "mod/mod_manifest.h"
#include "util/dir_walk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <windows.h>
#endif

/* 确保 path 的父目录存在（跨平台，支持绝对路径） */
static int ensure_parent(const char *path) {
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *slash = strrchr(tmp, '/');
    if (!slash) return RP_OK;          /* 无目录分量 */
    *slash = '\0';                     /* 截断到父目录 */
    for (char *p = tmp + 1; *p; p++) { /* 跳过开头的 '/'，逐级创建 */
        if (*p == '/') {
            *p = '\0';
#ifndef _WIN32
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return RP_ERR_IO;
#else
            if (_mkdir(tmp) != 0 && errno != EEXIST) return RP_ERR_IO;
#endif
            *p = '/';
        }
    }
#ifndef _WIN32
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return RP_ERR_IO;
#else
    if (_mkdir(tmp) != 0 && errno != EEXIST) return RP_ERR_IO;
#endif
    return RP_OK;
}

/* 拷贝单个文件（覆盖），确保父目录存在 */
static int copy_file(const char *src, const char *dst) {
    if (ensure_parent(dst) != RP_OK) return RP_ERR_IO;
    FILE *f = fopen(src, "rb");
    if (!f) return RP_ERR_IO;
    FILE *o = fopen(dst, "wb");
    if (!o) { fclose(f); return RP_ERR_IO; }
    uint8_t buf[65536];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), f)) > 0) fwrite(buf, 1, r, o);
    fclose(f); fclose(o);
    return RP_OK;
}

int merge_mod_into_dir(const char *game_dir, const char *mod_dir) {
    file_entry_t *files = NULL;
    size_t n = 0;
    int rc = collect_files(mod_dir, &files, &n);
    if (rc != RP_OK) return rc;

    for (size_t i = 0; i < n; i++) {
        char dst[2048];
        snprintf(dst, sizeof(dst), "%s/%s", game_dir, files[i].rel_path);
        rc = copy_file(files[i].full_path, dst);
        if (rc != RP_OK) {
            fprintf(stderr, "merge 失败: %s\n", files[i].rel_path);
            break;
        }
        printf("  merged: %s\n", files[i].rel_path);
    }
    free_files(files, n);
    return rc;
}

/* 模块实例 */
const mod_module_t g_mod_module = {
    .id = "merge",
    .desc = "文件级 mod 合并（同名覆盖/新增追加，两种注入共用）",
    .merge = merge_mod_into_dir,
};
