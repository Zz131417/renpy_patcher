/* util/credit.c —— 见 util/credit.h
 *
 * 给（已）注入/汉化的游戏包写入「由 Ren'Py Patcher 开源项目注入/汉化」的声明文字，
 * 便于汉化者/玩家追溯补丁来源。声明文字含项目名、仓库、版本、非商业许可证与生成时间。
 */
#include "util/credit.h"
#include "core/plugin.h"
#include "core/plugin_loader.h"
#include "core/version.h"
#include "util/log.h"
#include "rpa/rpa_v3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>      /* getpid */
#else
#include <direct.h>
#include <process.h>     /* getpid on Windows */
#endif

const char *CREDIT_FILENAME = "ZZ_RENPY_PATCHER_CREDIT.txt";

int credit_generate_text(char *buf, size_t n, const char *game_name) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32]; ts[0] = '\0';
    if (tm) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

    const char *g = (game_name && *game_name) ? game_name : "(unknown)";
    int w = snprintf(buf, n,
        "==============================================================\n"
        " Ren'Py Patcher - Injection / Translation Credit\n"
        "==============================================================\n"
        "本游戏内容由 Ren'Py Patcher 开源项目注入 / 汉化。\n"
        "This game content was injected / translated by Ren'Py Patcher (open-source).\n"
        "\n"
        "项目 / Project  : Ren'Py Patcher\n"
        "地址 / URL       : https://github.com/Zz131417/renpy_patcher\n"
        "版本 / Version   : %s\n"
        "许可证 / License : Non-Commercial License (commercial use prohibited)\n"
        "目标游戏 / Game  : %s\n"
        "生成时间 / Time   : %s\n"
        "\n"
        "Ren'Py Patcher 是一个面向 Ren'Py 游戏的非商业汉化 / Mod 补丁工具。\n"
        "Ren'Py Patcher is a non-commercial translation/mod patching tool for Ren'Py games.\n"
        "==============================================================\n",
        RPATCHER_VERSION_STRING, g, ts);
    return w > 0 ? RP_OK : RP_ERR_GENERIC;
}

int credit_write_file(const char *dir, const char *game_name) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", dir, CREDIT_FILENAME);
    FILE *f = fopen(path, "w");
    if (!f) { log_warn("无法写入来源声明文件: %s", path); return RP_ERR_IO; }
    char txt[2048];
    credit_generate_text(txt, sizeof(txt), game_name);
    fputs(txt, f);
    fclose(f);
    log_info("已写入来源声明: %s", path);
    return RP_OK;
}

/* 简易临时目录 + 清理（与 direct_inject 同思路，避免跨文件依赖）
 * 用进程相对的工作目录临时名，不依赖 Windows API（无需 windows.h）。 */
static void make_tmp(char *buf, size_t n, const char *suffix) {
    snprintf(buf, n, ".renpy_credit_tmp_%s_%u_%ld", suffix,
             (unsigned)getpid(), (long)time(NULL));
}
static void rm_tree(const char *dir) {
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

int credit_add_to_rpa(const char *in_rpa, const char *out_rpa, const char *game_name) {
    plugin_init();
    const rpa_handler_t *h = rpa_handler_for_file(in_rpa);
    if (!h || !h->unpack || !h->repack) {
        log_error("无法识别资源包或 handler 不支持: %s", in_rpa);
        return RP_ERR_NOTSUP;
    }

    char tmp[1024];
    make_tmp(tmp, sizeof(tmp), "add");
#ifndef _WIN32
    mkdir(tmp, 0755);
#else
    _mkdir(tmp);
#endif

    int rc = h->unpack(in_rpa, tmp);
    if (rc != RP_OK) { log_error("解包失败(%d)", rc); goto cleanup; }

    credit_write_file(tmp, game_name);

    rc = h->repack(tmp, out_rpa);
    if (rc != RP_OK) { log_error("重打包失败(%d)", rc); goto cleanup; }
    log_info("来源声明已烘焙进: %s", out_rpa);

cleanup:
    rm_tree(tmp);
    return rc;
}
