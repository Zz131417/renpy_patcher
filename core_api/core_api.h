/* core_api/core_api.h —— 供 GUI（WPF 等）通过 P/Invoke 调用的高层 C API
 *
 * 本文件导出一组 extern "C" 函数，封装底层核心模块（解包/重打包/mod/来源声明/
 * 导出/分支导图/翻译/游戏管理等）。所有字符串参数均为 UTF-8。
 *
 * 进度与日志通过回调上抛：GUI 可注册 log 回调实时显示日志、progress 回调显示进度。
 */
#ifndef RP_CORE_API_H
#define RP_CORE_API_H

#include <stddef.h>

#ifdef _WIN32
  #ifdef RP_CORE_EXPORT
    #define RP_CORE_API __declspec(dllexport)
  #else
    #define RP_CORE_API __declspec(dllimport)
  #endif
#else
  #define RP_CORE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 日志回调：level 同 log.h（0=DEBUG..3=ERROR）；msg 为 UTF-8 单行文本（不含换行）。 */
typedef void (*rp_log_cb)(int level, const char *msg, void *user);
/* 进度回调：percent 0..100；stage 为当前阶段描述（UTF-8）。 */
typedef void (*rp_progress_cb)(int percent, const char *stage, void *user);
/* 游戏遍历回调：返回非 0 可提前结束遍历。 */
typedef int  (*rp_game_cb)(const char *name, const char *path, const char *note, void *user);

/* 注册日志回调（传 NULL 取消）。设置后核心日志除写文件/stderr 外还会回调。 */
RP_CORE_API void rp_set_log_callback(rp_log_cb cb, void *user);
/* 注册进度回调（传 NULL 取消）。 */
RP_CORE_API void rp_set_progress_callback(rp_progress_cb cb, void *user);
/* 设置日志最低级别（LOG_DEBUG..LOG_ERROR）。 */
RP_CORE_API void rp_set_log_level(int level);
/* 最近一条错误详情（UTF-8）。 */
RP_CORE_API const char *rp_error_message(void);
/* 版本字符串，如 "renpy_patcher 1.3.0"。 */
RP_CORE_API const char *rp_version_string(void);

/* ---------- 游戏管理（别名登记） ---------- */
RP_CORE_API void rp_game_init(const char *store_path);   /* NULL=默认位置 */
RP_CORE_API int  rp_game_add(const char *name, const char *path, const char *note);
RP_CORE_API int  rp_game_remove(const char *name);
RP_CORE_API int  rp_game_count(void);
RP_CORE_API int  rp_game_list(rp_game_cb cb, void *user);
RP_CORE_API const char *rp_game_resolve(const char *token); /* 别名→路径，否则原样返回 */

/* ---------- 资源操作 ---------- */
/* 解包 .rpa（token 可为别名或路径）→ 输出目录 */
RP_CORE_API int rp_unpack(const char *game_token, const char *out_dir);
/* 把解包目录重打包为 .rpa */
RP_CORE_API int rp_build(const char *dir, const char *out_rpa);
/* 把 mod 目录合并进【已解包目录】dir（同名覆盖、新增追加） */
RP_CORE_API int rp_mod(const char *dir, const char *mod_dir);
/* drop-in 模式：把 mod 装入已解包游戏 game/ 下，生成 game/mod/ 与 game/dll/ 并写入
 * bootstrap（zzz_renpy_patcher.rpy）；游戏原 exe 启动时自动读取 mod/ 资源与 dll/。
 * mod_src / dll_dir 可为 NULL（仅写 bootstrap）。 */
RP_CORE_API int rp_mod_dropin(const char *game_dir, const char *mod_src, const char *dll_dir);

/* ---------- 进程级 DLL 注入（Windows） ---------- */
/* 把 dll_path 注入 pid 进程（经典 LoadLibrary 远程线程注入）。非 Windows 返回 RP_ERR_NOTSUP。 */
RP_CORE_API int rp_inject_dll(int pid, const char *dll_path);
/* 启动 exe_path 并把 dll_path 注入新进程；delay_ms<=0 挂起注入后恢复，>0 则等待后注入。 */
RP_CORE_API int rp_inject_dll_launch(const char *exe_path, const char *dll_path, int delay_ms);

/* ---------- 直接注入（烘焙）：把 DLL 放进游戏包并重打包 ---------- */
/* 解包 game_rpa → 把 dll_path 放入游戏目录 → 写来源声明 → 重打包为 out_rpa。跨平台。 */
RP_CORE_API int rp_inject_dll_bake(const char *game_rpa, const char *dll_path, const char *out_rpa);
/* 为 .rpa 追加来源声明，写出新 rpa */
RP_CORE_API int rp_credit_add(const char *game_token, const char *out_rpa, const char *game_name);
/* 仅生成来源声明文本（返回指针，UTF-8；下次调用前有效） */
RP_CORE_API const char *rp_credit_text(const char *game_name);

/* ---------- 导出 ---------- */
/* 把目录导出为 zip（split_mb>0 则分卷，单位 MB） */
RP_CORE_API int rp_export(const char *dir, const char *out_path, int split_mb);
/* 基于已有 APK 重打包（注入 mod，生成未签名 APK）。strip_sig 非 0 时移除旧签名。 */
RP_CORE_API int rp_export_apk(const char *apk, const char *mod_dir, const char *out_apk, int strip_sig);

/* ---------- 聊天/选择分支导图 ---------- */
/* fmt: "mermaid" | "json" | "dot" | "tree" */
RP_CORE_API int rp_branch(const char *dir, const char *out_path, const char *fmt);

/* ---------- 翻译流水线 ---------- */
RP_CORE_API int rp_translate_extract(const char *dir, const char *out_json);
RP_CORE_API int rp_translate_ai(const char *src_json, const char *out_json,
                                const char *from, const char *to,
                                const char *base_url, const char *api_key, const char *model);
RP_CORE_API int rp_translate_build(const char *json, const char *out_dir, const char *lang);

#ifdef __cplusplus
}
#endif
#endif /* RP_CORE_API_H */
