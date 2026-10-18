/* core_api/core_api.c —— 高层 C API 实现（供 GUI P/Invoke）
 *
 * 复用底层核心模块：rpa_v3 / mod_manifest / credit / export_module / apk_tool /
 * branch / translate_cli。翻译命令直接复用 DLL 内已编入的 cmd_translate。
 */
#include "core_api/core_api.h"

#include "util/log.h"
#include "core/game_mgr.h"
#include "rpa/rpa_v3.h"
#include "mod/mod_manifest.h"
#include "util/credit.h"
#include "inject/export/export_module.h"
#include "apk/apk_tool.h"
#include "analyze/branch.h"
#include "translate/translate_cli.h"   /* cmd_translate，已在 DLL 内 */
#include "core/version.h"

/* ---------------- 回调转发 ---------------- */
static rp_log_cb       g_log = NULL;
static void           *g_log_user = NULL;
static rp_progress_cb  g_prog = NULL;
static void           *g_prog_user = NULL;

static void log_forward(int level, const char *msg) {
    if (g_log) g_log(level, msg, g_log_user);
}
static void prog(int p, const char *s) {
    if (g_prog) g_prog(p, s ? s : "", g_prog_user);
}

RP_CORE_API void rp_set_log_callback(rp_log_cb cb, void *user) {
    g_log = cb; g_log_user = user;
    log_set_callback(cb ? log_forward : NULL);
}
RP_CORE_API void rp_set_progress_callback(rp_progress_cb cb, void *user) {
    g_prog = cb; g_prog_user = user;
}
RP_CORE_API void rp_set_log_level(int level) { log_set_level(level); }
RP_CORE_API const char *rp_error_message(void) { return rp_last_error(); }
RP_CORE_API const char *rp_version_string(void) {
    return "renpy_patcher " RPATCHER_VERSION_STRING;
}

/* ---------------- 游戏管理 ---------------- */
RP_CORE_API void rp_game_init(const char *store_path) { game_mgr_init(store_path); }
RP_CORE_API int  rp_game_add(const char *name, const char *path, const char *note) {
    return game_add(name, path, note ? note : "");
}
RP_CORE_API int  rp_game_remove(const char *name) { return game_remove(name); }
RP_CORE_API int  rp_game_count(void) { return (int)game_count(); }
RP_CORE_API const char *rp_game_resolve(const char *token) { return game_resolve(token); }

typedef struct { rp_game_cb cb; void *user; } game_enum_t;
static int enum_games(const game_entry_t *g, void *udata) {
    game_enum_t *e = (game_enum_t *)udata;
    return e->cb(g->name, g->path, g->note, e->user);
}
RP_CORE_API int rp_game_list(rp_game_cb cb, void *user) {
    game_enum_t e = { cb, user };
    return (int)game_list(enum_games, &e);
}

/* ---------------- 资源操作 ---------------- */
RP_CORE_API int rp_unpack(const char *game_token, const char *out_dir) {
    prog(0, "开始解包");
    const char *in = game_resolve(game_token);
    if (!in || !*in) { rp_set_error("无法解析游戏来源: %s", game_token); return RP_ERR_NOTSUP; }
    int rc = rpa_v3_unpack(in, out_dir);
    prog(100, rc == RP_OK ? "解包完成" : "解包失败");
    return rc;
}

RP_CORE_API int rp_build(const char *dir, const char *out_rpa) {
    prog(0, "开始重打包");
    int rc = rpa_v3_repack(dir, out_rpa);
    prog(100, rc == RP_OK ? "重打包完成" : "重打包失败");
    return rc;
}

RP_CORE_API int rp_mod(const char *dir, const char *mod_dir) {
    prog(0, "注入 mod");
    int rc = merge_mod_into_dir(dir, mod_dir);
    prog(100, rc == RP_OK ? "mod 注入完成" : "mod 注入失败");
    return rc;
}

RP_CORE_API int rp_credit_add(const char *game_token, const char *out_rpa, const char *game_name) {
    prog(0, "添加来源声明");
    const char *in = game_resolve(game_token);
    if (!in || !*in) { rp_set_error("无法解析游戏来源: %s", game_token); return RP_ERR_NOTSUP; }
    int rc = credit_add_to_rpa(in, out_rpa, game_name ? game_name : game_token);
    prog(100, rc == RP_OK ? "来源声明已添加" : "来源声明失败");
    return rc;
}

RP_CORE_API const char *rp_credit_text(const char *game_name) {
    static char buf[2048];
    credit_generate_text(buf, sizeof(buf), game_name);
    return buf;
}

/* ---------------- 导出 ---------------- */
RP_CORE_API int rp_export(const char *dir, const char *out_path, int split_mb) {
    prog(0, "导出中");
    size_t split = split_mb > 0 ? (size_t)split_mb * 1024 * 1024 : 0;
    int rc = export_dir(dir, out_path, split, 1);
    prog(100, rc == RP_OK ? "导出完成" : "导出失败");
    return rc;
}

RP_CORE_API int rp_export_apk(const char *apk, const char *mod_dir, const char *out_apk, int strip_sig) {
    prog(0, "重打包 APK");
    int rc = apk_repack(apk, mod_dir, out_apk, strip_sig ? 1 : 0);
    prog(100, rc == RP_OK ? "APK 重打包完成" : "APK 重打包失败");
    return rc;
}

/* ---------------- 分支导图 ---------------- */
RP_CORE_API int rp_branch(const char *dir, const char *out_path, const char *fmt) {
    prog(0, "解析分支");
    branch_graph_t g;
    if (analyze_branches(dir, &g) != RP_OK) { prog(100, "解析失败"); return RP_ERR_FORMAT; }
    const char *f = fmt ? fmt : "mermaid";
    int rc = branch_export(&g, f, out_path);
    branch_free(&g);
    prog(100, rc == RP_OK ? "导图已生成" : "导图生成失败");
    return rc;
}

/* ---------------- 翻译（复用 DLL 内 cmd_translate） ---------------- */
static int run_translate(int argc, char **argv) {
    prog(0, "翻译处理中");
    int rc = cmd_translate(argc, argv);
    prog(100, rc == RP_OK ? "完成" : "失败");
    return rc;
}

RP_CORE_API int rp_translate_extract(const char *dir, const char *out_json) {
    char *argv[] = { "extract", (char *)dir, "-o", (char *)out_json };
    return run_translate(4, argv);
}

RP_CORE_API int rp_translate_ai(const char *src_json, const char *out_json,
                                const char *from, const char *to,
                                const char *base_url, const char *api_key, const char *model) {
    /* 动态拼 argv，跳过 NULL 项 */
    char *argv[16];
    int n = 0;
    argv[n++] = "ai";
    argv[n++] = (char *)src_json;
    argv[n++] = "-o"; argv[n++] = (char *)out_json;
    if (from)     { argv[n++] = "--from";     argv[n++] = (char *)from; }
    if (to)       { argv[n++] = "--to";       argv[n++] = (char *)to; }
    if (base_url) { argv[n++] = "--ai-base";  argv[n++] = (char *)base_url; }
    if (api_key)  { argv[n++] = "--ai-key";   argv[n++] = (char *)api_key; }
    if (model)    { argv[n++] = "--ai-model"; argv[n++] = (char *)model; }
    return run_translate(n, argv);
}

RP_CORE_API int rp_translate_build(const char *json, const char *out_dir, const char *lang) {
    char *argv[] = { "build", (char *)json, "--lang", (char *)lang, "-o", (char *)out_dir };
    return run_translate(6, argv);
}
