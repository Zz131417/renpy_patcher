/* inject/direct/direct_inject.c —— 见 direct_inject.h */
#include "inject/direct/direct_inject.h"
#include "core/plugin_loader.h"
#include "rpa/rpa_v3.h"
#include "mod/mod_manifest.h"
#include "util/credit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

/* 生成唯一临时目录名（跨平台） */
static void make_tmp_name(char *buf, size_t n, const char *suffix) {
#ifdef _WIN32
    char tp[MAX_PATH];
    GetTempPathA(MAX_PATH, tp);
    snprintf(buf, n, "%srenpy_patch_%s_%u", tp, suffix, (unsigned)GetCurrentProcessId());
#else
    snprintf(buf, n, "/tmp/renpy_patch_%s_%u_%ld", suffix,
             (unsigned)getpid(), (long)time(NULL));
#endif
}

/* 递归删除目录（跨平台，用于清理临时目录） */
static void remove_tree(const char *dir) {
#ifdef _WIN32
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmd /c rmdir /s /q \"%s\"", dir);
    system(cmd);
#else
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", dir);
    system(cmd);
#endif
}

int direct_inject_build(const char *game_rpa,
                        const char *mod_dir,
                        const char *out_rpa) {
    plugin_init();

    const rpa_handler_t *h = rpa_handler_for_file(game_rpa);
    if (!h) {
        fprintf(stderr, "错误：无法识别资源包: %s\n", game_rpa);
        return RP_ERR_FORMAT;
    }
    if (!h->unpack || !h->repack) {
        fprintf(stderr, "错误：handler 未实现 unpack/repack\n");
        return RP_ERR_NOTSUP;
    }

    char tmp[1024];
    make_tmp_name(tmp, sizeof(tmp), "inject");
#ifndef _WIN32
    mkdir(tmp, 0755);
#else
    _mkdir(tmp);
#endif

    int rc = h->unpack(game_rpa, tmp);
    if (rc != RP_OK) { fprintf(stderr, "解包失败(%d)\n", rc); goto cleanup; }

    rc = merge_mod_into_dir(tmp, mod_dir);
    if (rc != RP_OK) { fprintf(stderr, "合并 mod 失败(%d)\n", rc); goto cleanup; }

    /* 自动写入来源声明（由 Ren'Py Patcher 注入/汉化） */
    credit_write_file(tmp, game_rpa);

    rc = h->repack(tmp, out_rpa);
    if (rc != RP_OK) { fprintf(stderr, "重打包失败(%d)\n", rc); goto cleanup; }

    printf("direct-inject 完成: %s\n", out_rpa);

cleanup:
    remove_tree(tmp);
    return rc;
}

/* 模块 run：build <game.rpa> -m <mod_dir> -o <out_rpa> */
static int run(int argc, char **argv) {
    const char *game = NULL, *mod = NULL, *out = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-m") && i + 1 < argc) mod = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!game) game = argv[i];
    }
    if (!game || !mod) { fprintf(stderr, "用法: build <game.rpa> -m <mod_dir> -o <out.rpa>\n"); return 2; }
    if (!out) out = "patched.rpa";
    return direct_inject_build(game, mod, out);
}

const inject_module_t g_inject_direct = {
    .id = "direct",
    .desc = "直接注入：解包→合并mod→重打包（烘焙进包）",
    .run = run,
};
