/*
 * cli/main.c —— 命令行入口
 * 通过中央模块注册表（plugin_loader）按子命令分发，体现可扩展架构。
 *
 * 用法：
 *   patcher [全局选项] <子命令> [子命令参数...]
 *
 *   全局选项（须置于子命令之前）：
 *     --version / -v        查看版本与构建信息
 *     --log <file>          指定日志文件路径（默认 patcher.log）
 *     --no-log             不写日志文件，仅输出到 stderr
 *     --debug               打开 DEBUG 级别日志
 *     --log-level <0-3>     设置最低日志级别（0=DEBUG 1=INFO 2=WARN 3=ERROR）
 *
 *   子命令（argv[0]=子命令名）：
 *     unpack   <game.rpa|游戏名> -o <out_dir>
 *     repack   <dir> -o <out.rpa>
 *     build    <game.rpa|游戏名> -m <mod_dir> -o <out.rpa>
 *     hotinject --pid <pid> --mod <mod_dir>
 *     game add     <name> <path> [note]     登记游戏
 *     game list                              列出已登记游戏
 *     game show    <name>                   查看游戏详情（含文件数）
 *     game remove  <name>                   注销游戏
 *     game path    <name>                   仅打印游戏路径（便于脚本）
 *     export   <dir> -f 7z -split 100M      （里程碑5，待实现）
 *
 * 约定：进入子命令后 argv[0] 即子命令名本身，argv[1..] 为其参数。
 */
#include "core/plugin_loader.h"
#include "core/plugin.h"
#include "core/game_mgr.h"
#include "rpa/rpa_v3.h"
#include "util/credit.h"
#include "inject/export/export_module.h"
#include "translate/translate_cli.h"
#include "util/log.h"
#include "util/zip_extract.h"
#include "util/sfx.h"
#include "util/resolve_source.h"
#include "util/dir_walk.h"

/* 前向声明（定义在文件后部，被 cmd_unpack/cmd_inject 提前调用） */
static const char *resolve_input_rpa(const char *token, char *tmpdir, size_t tn, char *rpa, size_t rn);
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void print_version(void) {
    printf("Ren'Py Patcher %s\n", RPATCHER_VERSION_STRING);
    printf("build:   %s\n", RPATCHER_VERSION_FULL);
    printf("commit:  %s\n", RPATCHER_BUILD_COMMIT);
    printf("license: Non-Commercial (see LICENSE)\n");
}

static void usage(const char *prog) {
    printf("Ren'Py Patcher %s\n", RPATCHER_VERSION_STRING);
    printf("用法:\n");
    printf("  %s unpack   <game.rpa|游戏名> -o <out_dir>\n", prog);
    printf("  %s repack   <dir> -o <out.rpa>\n", prog);
    printf("  %s build    <game.rpa|游戏名> -m <mod_dir> -o <out.rpa>\n", prog);
    printf("  %s hotinject --pid <pid> --mod <mod_dir>\n", prog);
    printf("  %s game add <name> <path> [note]\n", prog);
    printf("  %s game list | show <name> | remove <name> | path <name>\n", prog);
    printf("  %s credit <game.rpa|游戏名> [-o out.rpa]   给包添加来源声明\n", prog);
    printf("  %s credit --text [--game <名>]                仅导出声明文字\n", prog);
    printf("  %s export <dir> -o <out.zip> [-split 100M] [-store|-deflate]   导出 zip（可分卷）\n", prog);
    printf("全局选项: --log <file> | --no-log | --debug | --log-level <0-3>\n");
}

/* 解析 -o / -m / --pid / --mod 等简单键值对（从 start 起） */
static const char *get_opt(int argc, char **argv, int start, const char *key) {
    for (int i = start; i + 1 < argc; i++)
        if (strcmp(argv[i], key) == 0) return argv[i + 1];
    return NULL;
}

/* ---------- 资源包子命令 ---------- */

int cmd_unpack(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    const char *token = argv[1];
    const char *out   = get_opt(argc, argv, 2, "-o");
    if (!out) out = "out";
    char _td[2048], _rpa[2048];
    const char *in = resolve_input_rpa(token, _td, sizeof _td, _rpa, sizeof _rpa);
    if (!in) { fprintf(stderr, "无法解析游戏来源（压缩包内未找到 .rpa？）\n"); return 2; }

    log_info("开始解包: %s -> %s", in, out);
    plugin_init();
    const rpa_handler_t *h = rpa_handler_for_file(in);
    if (!h) { log_error("无法识别的资源包格式: %s", in); return 2; }
    log_info("handler: %s (%s)", h->id, h->desc ? h->desc : "");
    int rc = h->unpack(in, out);
    if (rc != RP_OK) { log_error("解包失败，错误码 %d", rc); return 3; }
    log_info("解包完成 -> %s", out);
    return 0;
}

int cmd_repack(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    const char *in  = game_resolve(argv[1]);
    const char *out = get_opt(argc, argv, 2, "-o");
    if (!out) out = "out.rpa";

    log_info("开始重打包: %s -> %s", in, out);
    plugin_init();
    const rpa_handler_t *h = rpa_handler_get("rpa.v3");
    if (!h || !h->repack) { log_error("未找到 rpa.v3 重打包 handler"); return 2; }
    int rc = h->repack(in, out);
    if (rc != RP_OK) { log_error("重打包失败，错误码 %d", rc); return 3; }
    log_info("重打包完成 -> %s", out);
    return 0;
}

/* argv[0]=子命令名；argv[1..]=参数。direct: argv[1] 是游戏（支持别名） */
int cmd_inject(int argc, char **argv, const char *id) {
    if (argc < 2) { usage(argv[0]); return 1; }
    plugin_init();
    const inject_module_t *m = inject_module_get(id);
    if (!m) { log_error("未找到注入模块 %s", id); return 2; }
    if (strcmp(id, "direct") == 0 && argc > 1) {
        char _td[2048], _rpa[2048];
        const char *r = resolve_input_rpa(argv[1], _td, sizeof _td, _rpa, sizeof _rpa);
        if (r) argv[1] = (char *)r;
        else { log_error("无法解析游戏来源（压缩包内未找到 .rpa？）"); return 2; }
    }
    log_info("调用注入模块: %s", id);
    return m->run(argc - 1, argv + 1);
}

int cmd_credit(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "用法: credit <game.rpa|游戏名> [-o out.rpa] | credit --text [--game <名>]\n"); return 1; }
    if (!strcmp(argv[1], "--text")) {
        const char *g = get_opt(argc, argv, 2, "--game");
        char buf[2048];
        credit_generate_text(buf, sizeof(buf), g);
        fputs(buf, stdout);
        return 0;
    }
    const char *token = argv[1];
    const char *out   = get_opt(argc, argv, 2, "-o");
    if (!out) out = "credited.rpa";
    const char *in = game_resolve(token);
    log_info("为 rpa 添加来源声明: %s -> %s", in, out);
    int rc = credit_add_to_rpa(in, out, token);
    if (rc != RP_OK) { log_error("添加声明失败，错误码 %d", rc); return rc; }
    printf("已添加来源声明 -> %s\n", out);
    return 0;
}


/* ---------- 游戏管理子命令 ---------- */

static int list_cb(const game_entry_t *g, void *udata) {
    (void)udata;
    printf("  %-20s %s\n", g->name, g->path);
    if (g->note[0]) printf("      备注: %s\n", g->note);
    return 0;
}

int cmd_game(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    const char *sub = argv[1];

    if (!strcmp(sub, "add")) {
        if (argc < 4) { fprintf(stderr, "用法: game add <name> <path> [note]\n"); return 1; }
        const char *note = (argc > 4) ? argv[4] : "";
        int rc = game_add(argv[2], argv[3], note);
        if (rc != RP_OK) { log_error("添加游戏失败，错误码 %d", rc); return rc; }
        log_info("已登记游戏: %s -> %s", argv[2], argv[3]);
        printf("已添加游戏: %s\n", argv[2]);
        return 0;
    }
    if (!strcmp(sub, "list")) {
        size_t n = game_list(list_cb, NULL);
        log_info("当前已登记 %zu 个游戏", n);
        if (n == 0) printf("(无已登记游戏)\n");
        return 0;
    }
    if (!strcmp(sub, "show")) {
        if (argc < 3) { fprintf(stderr, "用法: game show <name>\n"); return 1; }
        const game_entry_t *g = game_get(argv[2]);
        if (!g) { fprintf(stderr, "未找到游戏: %s\n", argv[2]); return 1; }
        printf("游戏: %s\n", g->name);
        printf("路径: %s\n", g->path);
        if (g->note[0]) printf("备注: %s\n", g->note);
        printf("登记时间: %lld\n", (long long)g->added);
        size_t nfiles = 0;
        if (strstr(g->path, ".rpa") && rpa_v3_stat(g->path, &nfiles) == RP_OK)
            printf("包含文件数: %zu\n", nfiles);
        log_info("查看游戏详情: %s", g->name);
        return 0;
    }
    if (!strcmp(sub, "remove")) {
        if (argc < 3) { fprintf(stderr, "用法: game remove <name>\n"); return 1; }
        int rc = game_remove(argv[2]);
        if (rc != RP_OK) { fprintf(stderr, "未找到或无法删除: %s\n", argv[2]); return 1; }
        log_info("已注销游戏: %s", argv[2]);
        printf("已删除游戏: %s\n", argv[2]);
        return 0;
    }
    if (!strcmp(sub, "path")) {
        if (argc < 3) { fprintf(stderr, "用法: game path <name>\n"); return 1; }
        const game_entry_t *g = game_get(argv[2]);
        if (!g) { fprintf(stderr, "未找到游戏: %s\n", argv[2]); return 1; }
        printf("%s\n", g->path);
        return 0;
    }
    fprintf(stderr, "未知 game 子命令: %s\n", sub);
    return 1;
}

/* ---------- 平台相关辅助（导入压缩包 / 定位桩） ---------- */
#ifdef _WIN32
#include <windows.h>
static int make_tmp_dir(char *buf, size_t n) {
    char base[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, base)) return -1;
    snprintf(buf, n, "%srenpy_patcher_%u", base, GetCurrentProcessId());
    return CreateDirectoryA(buf, NULL) ? 0 : -1;
}
static void get_exe_dir(char *buf, size_t n) {
    char self[MAX_PATH];
    GetModuleFileNameA(NULL, self, MAX_PATH);
    char drive[MAX_PATH], d[MAX_PATH], fn[MAX_PATH], ex[MAX_PATH];
    _splitpath(self, drive, d, fn, ex);
    snprintf(buf, n, "%s%s", drive, d);
}
#else
#include <sys/stat.h>
#include <unistd.h>
static int make_tmp_dir(char *buf, size_t n) {
    snprintf(buf, n, "/tmp/renpy_patcher_%d", (int)getpid());
    return mkdir(buf, 0700);
}
#endif

/* 把输入（.rpa/别名/整包.zip）归一化为可直接处理的 .rpa 路径。
 * 若是 zip，解压到 tmpdir 并定位内部 .rpa 写入 rpa 缓冲。返回路径或 NULL。 */
static const char *resolve_input_rpa(const char *token, char *tmpdir, size_t tn, char *rpa, size_t rn) {
    const char *resolved = game_resolve(token);
    if (!is_zip_file(resolved)) return resolved;
    if (make_tmp_dir(tmpdir, tn) != 0) { log_error("无法创建临时目录用于解压"); return NULL; }
    int rc = resolve_game_source(token, tmpdir, rpa, rn);
    if (rc != RP_OK || rpa[0] == '\0') { log_error("压缩包内未找到 .rpa 资源包"); return NULL; }
    return rpa;
}

#ifdef _WIN32
static char g_stub_buf[2048];
#endif
static const char *pick_stub(void) {
#ifdef _WIN32
    char d[2048]; get_exe_dir(d, sizeof(d));
    snprintf(g_stub_buf, sizeof(g_stub_buf), "%s\\sfx_stub.exe", d);
    FILE *f = fopen(g_stub_buf, "rb");
    if (f) { fclose(f); return g_stub_buf; }
#endif
    return NULL;
}

/* ---------- 导入 / 导出 exe 子命令 ---------- */

int cmd_import(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    const char *zip = argv[1];
    const char *out = get_opt(argc, argv, 2, "-o");
    if (!out) out = "imported_game";
    const char *name = get_opt(argc, argv, 2, "--name");

    int cnt = 0;
    int rc = zip_extract(zip, out, &cnt);
    if (rc != RP_OK) { fprintf(stderr, "解压游戏压缩包失败，错误码 %d\n", rc); return 3; }

    if (name) {
        char rpa[2048]; rpa[0] = '\0';
        file_entry_t *files = NULL; size_t n = 0;
        if (collect_files(out, &files, &n) == RP_OK) {
            for (size_t i = 0; i < n; i++) {
                size_t L = strlen(files[i].full_path);
                if (L > 4 && strcmp(files[i].full_path + L - 4, ".rpa") == 0) {
                    snprintf(rpa, sizeof(rpa), "%s", files[i].full_path); break;
                }
            }
            free_files(files, n);
        }
        const char *path = rpa[0] ? rpa : out;
        int g = game_add(name, path, "");
        if (g != RP_OK) { fprintf(stderr, "登记游戏失败，错误码 %d\n", g); return 4; }
        printf("已导入并登记游戏 %s -> %s\n", name, path);
    } else {
        printf("已解压游戏到 %s（%d 个文件）\n", out, cnt);
    }
    return 0;
}

int cmd_export_exe(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    const char *dir = game_resolve(argv[1]);
    const char *out = get_opt(argc, argv, 2, "-o");
    if (!out) out = "setup.exe";
    const char *launcher = get_opt(argc, argv, 2, "--launcher");
    const char *stub_arg = get_opt(argc, argv, 2, "--stub");
    int compress = 1;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--store")) compress = 0;
        if (!strcmp(argv[i], "--deflate")) compress = 1;
    }
    const char *stub = stub_arg ? stub_arg : pick_stub();
    int rc = sfx_build(stub, dir, out, launcher, compress);
    if (rc == RP_ERR_NOTSUP) {
        fprintf(stderr, "仅 Windows 支持生成 exe（需要 sfx_stub.exe 自解压桩）\n");
        return 4;
    }
    if (rc != RP_OK) { fprintf(stderr, "生成 exe 失败，错误码 %d\n", rc); return 3; }
    printf("已生成%s -> %s\n", launcher ? "启动器 exe" : "自解压安装包 exe", out);
    return 0;
}

/* ---------- 入口 ---------- */

int main(int argc, char **argv) {
    const char *prog = argv[0];

    /* 默认日志写入 patcher.log；可用 --no-log 关闭 */
    log_init("patcher.log");
    log_set_tag("main");

    /* 解析子命令之前的全局选项 */
    int i = 1;
    while (i < argc) {
        const char *a = argv[i];
        if (a[0] != '-') break;                       /* 遇到子命令，停止 */
        if (!strcmp(a, "--version") || !strcmp(a, "-v")) { print_version(); return 0; }
        else if (!strcmp(a, "--log") && i + 1 < argc) { log_init(argv[++i]); }
        else if (!strcmp(a, "--no-log")) { log_init(NULL); }
        else if (!strcmp(a, "--debug")) { log_set_level(LOG_DEBUG); }
        else if (!strcmp(a, "--log-level") && i + 1 < argc) { log_set_level(atoi(argv[++i])); }
        else break;                                  /* 未知全局选项，交给子命令 */
        i++;
    }

    if (i >= argc) { usage(prog); return 1; }

    const char *cmd = argv[i];
    int sub_argc = argc - i;          /* argv[i] 即子命令名当作 argv[0] */
    char **sub_argv = argv + i;

    game_mgr_init(NULL);              /* 初始化游戏登记表 */

    if (!strcmp(cmd, "unpack"))    return cmd_unpack(sub_argc, sub_argv);
    if (!strcmp(cmd, "repack"))    return cmd_repack(sub_argc, sub_argv);
    if (!strcmp(cmd, "build"))     return cmd_inject(sub_argc, sub_argv, "direct");
    if (!strcmp(cmd, "hotinject")) return cmd_inject(sub_argc, sub_argv, "hot");
    if (!strcmp(cmd, "game"))      return cmd_game(sub_argc, sub_argv);
    if (!strcmp(cmd, "credit"))    return cmd_credit(sub_argc, sub_argv);
    if (!strcmp(cmd, "export"))    return export_run(sub_argc, sub_argv);
    if (!strcmp(cmd, "import"))     return cmd_import(sub_argc, sub_argv);
    if (!strcmp(cmd, "export-exe")) return cmd_export_exe(sub_argc, sub_argv);
    if (!strcmp(cmd, "translate")) return cmd_translate(sub_argc, sub_argv);

    usage(prog);
    return 1;
}
