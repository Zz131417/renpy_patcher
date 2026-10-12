/* mod/mod_manifest.h —— 统一 mod 模块（文件级合并，两种注入共用） */
#ifndef RENPY_PATCHER_MOD_MANIFEST_H
#define RENPY_PATCHER_MOD_MANIFEST_H

#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 合并 mod_dir 进 game_dir（同名覆盖、新增追加），返回 RP_OK。 */
int merge_mod_into_dir(const char *game_dir, const char *mod_dir);

/* 注册的 mod 模块实例（供 plugin_loader 注册） */
extern const mod_module_t g_mod_module;

#ifdef __cplusplus
}
#endif
#endif
