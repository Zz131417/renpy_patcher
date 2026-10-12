/* inject/direct/direct_inject.h —— 直接注入（烘焙进包） */
#ifndef RENPY_PATCHER_DIRECT_INJECT_H
#define RENPY_PATCHER_DIRECT_INJECT_H

#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 直接注入一步到位：
 *   解包 game_rpa → 合并 mod_dir → 重打包为 out_rpa
 * 中间产物写入临时目录，完成后删除。
 */
int direct_inject_build(const char *game_rpa,
                        const char *mod_dir,
                        const char *out_rpa);

/* 注册的注入模块实例（供 plugin_loader 注册） */
extern const inject_module_t g_inject_direct;

#ifdef __cplusplus
}
#endif
#endif
