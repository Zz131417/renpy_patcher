/* inject/export/export_module.c —— 见 export_module.h
 *
 * `export` 命令：把一个目录（如解包产物、mod 目录）打包成标准 .zip，
 * 便于汉化补丁分发。可选 -split 分卷。底层用 util/zip.c（仅依赖 zlib）。
 */
#include "inject/export/export_module.h"
#include "core/plugin.h"
#include "core/game_mgr.h"
#include "util/zip.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>      /* _mkdir on Windows */
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFDIR) != 0)   /* MSVC 无 S_ISDIR 宏 */
#endif

/* 解析尺寸：100M / 1G / 500K / 纯数字（字节）。失败返回 0。 */
static size_t parse_size(const char *s) {
    if (!s || !*s) return 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return 0;
    unsigned long long mult = 1;
    if (*end) {
        switch (tolower((unsigned char)*end)) {
            case 'k': mult = 1024ULL; break;
            case 'm': mult = 1024ULL * 1024; break;
            case 'g': mult = 1024ULL * 1024 * 1024; break;
            case 't': mult = 1024ULL * 1024 * 1024 * 1024; break;
            default:  mult = 1; break;
        }
    }
    return (size_t)(v * mult);
}

/* 取简单键值选项（如 -o / -split），返回其后一个参数 */
static const char *get_opt(int argc, char **argv, int start, const char *key) {
    for (int i = start; i + 1 < argc; i++)
        if (strcmp(argv[i], key) == 0) return argv[i + 1];
    return NULL;
}

int export_dir(const char *dir, const char *out, size_t split_size, int compress) {
    if (!dir || !out || !*dir || !*out) return RP_ERR_GENERIC;

    /* 目录检查 */
#ifndef _WIN32
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
#else
    struct _stat st;
    if (_stat(dir, &st) != 0 || !(st.st_mode & _S_IFDIR)) {
#endif
        log_error("目录不存在或非目录: %s", dir);
        fprintf(stderr, "目录不存在: %s\n", dir);
        return RP_ERR_GENERIC;
    }

    log_info("导出目录为 zip: %s -> %s (compress=%d, split=%zu)", dir, out, compress, split_size);
    zip_writer_t *z = zip_open(out, split_size);
    if (!z) { log_error("zip_open 失败: %s", out); return RP_ERR_GENERIC; }

    int rc = zip_add_dir(z, dir, "", compress);
    if (rc != RP_OK) { zip_close(z); log_error("添加文件失败，错误码 %d", rc); return rc; }

    rc = zip_close(z);
    if (rc != RP_OK) { log_error("写出 zip 失败，错误码 %d", rc); return rc; }

    printf("已导出 -> %s%s\n", out, split_size ? " (分卷: .z01 ... .zip)" : "");
    return RP_OK;
}

int export_run(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "用法: export <dir> -o <out.zip> [-split 100M] [-store|-deflate]\n");
        return 1;
    }
    const char *dir = argv[1];
    const char *out = get_opt(argc, argv, 2, "-o");
    if (!out) out = "patch_export.zip";
    const char *split_s = get_opt(argc, argv, 2, "-split");
    int compress = 1;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-store"))   compress = 0;
        else if (!strcmp(argv[i], "-deflate")) compress = 1;
    }
    size_t split = split_s ? parse_size(split_s) : 0;
    return export_dir(dir, out, split, compress);
}
