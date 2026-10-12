/*
 * core/plugin_loader.h —— 模块注册与分发（中央注册表）
 *
 * plugin_init() 注册所有内置模块；未来可扩展为扫描 plugins/ 目录，
 * LoadLibrary 外部 DLL 并调用其 rp_register_modules() 完成热插拔。
 */
#ifndef RENPY_PATCHER_PLUGIN_LOADER_H
#define RENPY_PATCHER_PLUGIN_LOADER_H

#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 注册一个模块（类型 + id 作为主键）。重复 id 后者覆盖。 */
void rp_register_module(const rp_module_t *m);

/* 启动时调用：注册所有内置模块。应在任何 handler 查询前调用一次。 */
void plugin_init(void);

/* ---- RPA handler 查询（里程碑1/2） ---- */
const rpa_handler_t *rpa_handler_for_data(const uint8_t *hdr, size_t len);
const rpa_handler_t *rpa_handler_for_file(const char *path);
const rpa_handler_t *rpa_handler_get(const char *id);

/* ---- 扩展模块查询（为里程碑 3~9 预留统一入口） ---- */
const mod_module_t        *mod_module_get(const char *id);
const inject_module_t     *inject_module_get(const char *id);
const translator_backend_t *translator_get(const char *id);
const exporter_t          *exporter_get(const char *id);

#ifdef __cplusplus
}
#endif
#endif
