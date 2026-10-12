/*
 * core/plugin.h —— 统一模块/插件契约（可扩展架构）
 *
 * 设计目标：所有能力都以「模块(module)」形式注册到中央注册表，
 * 主程序按类型(type)查找并使用。新增能力只需实现对应 vtable 并注册，
 * 不改动调度代码——这是为里程碑 5(压缩导出)/7(汉化流水线)/8(翻译后端)/
 * 9(APK) 预留的扩展空间。
 *
 * 模块类型：
 *   RP_MOD_RPA        资源包格式 handler（里程碑1/2，已实现 rpa.v3）
 *   RP_MOD_MOD        统一 mod 模块（里程碑3/4，两种注入共用 mod 目录）
 *   RP_MOD_INJECT     注入器（直接注入 / 热注入）
 *   RP_MOD_TRANSLATOR 翻译后端（AI/人工/混合，里程碑7/8）
 *   RP_MOD_EXPORTER   压缩导出器（zip/7z/rar/分卷，里程碑5）
 */
#ifndef RENPY_PATCHER_PLUGIN_H
#define RENPY_PATCHER_PLUGIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 错误码（全局统一） */
#define RP_OK          0
#define RP_ERR_NOMEM   1
#define RP_ERR_IO      2
#define RP_ERR_FORMAT  3
#define RP_ERR_NOTSUP  4
#define RP_ERR_GENERIC 5
#define RP_ERR_PLUGIN  6

#include "core/version.h"   /* 版本号由 CMake 根据 VERSION 自动生成 */

/* ---- 资源包格式 handler（里程碑1/2） ---- */
typedef struct rpa_handler {
    const char *id;     /* 如 "rpa.v3" */
    const char *desc;   /* 人类可读描述 */
    int  (*can_handle)(const uint8_t *hdr, size_t len);
    int  (*unpack)(const char *in_file, const char *out_dir);
    int  (*repack)(const char *dir, const char *out_file);
} rpa_handler_t;

/* ---- Mod 模块（里程碑3/4）：两种注入共用的 mod 目录契约 ---- */
typedef struct mod_module {
    const char *id;
    const char *desc;
    /* 把 mod_dir 合并进 game_dir（同名覆盖、新增追加），返回 RP_OK */
    int (*merge)(const char *game_dir, const char *mod_dir);
} mod_module_t;

/* ---- 注入器（里程碑3 直接 / 4 热注入） ---- */
typedef struct inject_module {
    const char *id;     /* "direct" / "hot" */
    const char *desc;
    /* 解释 argv（argc/argv 已跳过子命令名），返回 RP_OK */
    int (*run)(int argc, char **argv);
} inject_module_t;

/* ---- 翻译后端（里程碑7/8 占位） ---- */
typedef struct translator_backend {
    const char *id;     /* "ai" / "human" / "hybrid" */
    const char *desc;
    int (*translate)(const char *in_text, char **out_text);  /* 未来实现 */
} translator_backend_t;

/* ---- 压缩导出器（里程碑5 占位） ---- */
typedef struct exporter {
    const char *id;     /* "zip" / "7z" / "rar" */
    const char *desc;
    int (*export_fn)(const char *dir, const char *out, const char *fmt, size_t split_bytes);
} exporter_t;

/* ---- 统一模块类型 ---- */
typedef enum {
    RP_MOD_RPA = 0,
    RP_MOD_MOD,
    RP_MOD_INJECT,
    RP_MOD_TRANSLATOR,
    RP_MOD_EXPORTER,
    RP_MOD_COUNT
} rp_module_type;

typedef struct rp_module {
    rp_module_type type;
    const char    *id;
    const char    *desc;
    union {
        const rpa_handler_t        *rpa;
        const mod_module_t         *mod;
        const inject_module_t      *inject;
        const translator_backend_t *translator;
        const exporter_t           *exporter;
    } as;
} rp_module_t;

#ifdef __cplusplus
}
#endif
#endif
