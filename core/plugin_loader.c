/* core/plugin_loader.c —— 见 plugin_loader.h */
#include "core/plugin_loader.h"
#include "rpa/rpa_v3.h"
#include "mod/mod_manifest.h"
#include "inject/direct/direct_inject.h"
#ifndef RP_NO_HOT_INJECT
#include "inject/hot/agent.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_MODULES 64
static rp_module_t g_modules[MAX_MODULES];
static int        g_nmods = 0;
static int        g_inited = 0;

void rp_register_module(const rp_module_t *m) {
    if (!m) return;
    /* 同类型同 id 覆盖 */
    for (int i = 0; i < g_nmods; i++) {
        if (g_modules[i].type == m->type && g_modules[i].id &&
            strcmp(g_modules[i].id, m->id) == 0) {
            g_modules[i] = *m;
            return;
        }
    }
    if (g_nmods < MAX_MODULES) g_modules[g_nmods++] = *m;
}

static const rp_module_t *find_mod(rp_module_type t, const char *id) {
    for (int i = 0; i < g_nmods; i++)
        if (g_modules[i].type == t && (!id || (g_modules[i].id && strcmp(g_modules[i].id, id) == 0)))
            return &g_modules[i];
    return NULL;
}

/* 内部：注册所有内置模块 */
static void register_builtins(void) {
    /* 里程碑1/2：RPA-3.0 解包/重打包 */
    static const rpa_handler_t v3 = {
        .id = "rpa.v3",
        .desc = "Ren'Py Archive 3.0",
        .can_handle = rpa_v3_can_handle,
        .unpack = rpa_v3_unpack,
        .repack = rpa_v3_repack,
    };
    rp_register_module(&(rp_module_t){ .type = RP_MOD_RPA, .id = v3.id, .desc = v3.desc, .as.rpa = &v3 });

    /* 里程碑3/4：统一 mod 模块（两种注入共用） */
    extern const mod_module_t g_mod_module;
    rp_register_module(&(rp_module_t){ .type = RP_MOD_MOD, .id = g_mod_module.id,
                                       .desc = g_mod_module.desc, .as.mod = &g_mod_module });

    /* 里程碑3：直接注入器 */
    extern const inject_module_t g_inject_direct;
    rp_register_module(&(rp_module_t){ .type = RP_MOD_INJECT, .id = g_inject_direct.id,
                                       .desc = g_inject_direct.desc, .as.inject = &g_inject_direct });

#ifndef RP_NO_HOT_INJECT
    /* 里程碑4：热注入器（Windows）；GUI 版可定义 RP_NO_HOT_INJECT 排除（避免依赖 libpython） */
    extern const inject_module_t g_inject_hot;
    rp_register_module(&(rp_module_t){ .type = RP_MOD_INJECT, .id = g_inject_hot.id,
                                       .desc = g_inject_hot.desc, .as.inject = &g_inject_hot });
#endif

    /* TODO 里程碑5/7/8：扫描 plugins/ 目录 LoadLibrary，调用 rp_register_modules() */
}

void plugin_init(void) {
    if (g_inited) return;
    register_builtins();
    g_inited = 1;
}

const rpa_handler_t *rpa_handler_for_data(const uint8_t *hdr, size_t len) {
    plugin_init();
    for (int i = 0; i < g_nmods; i++) {
        if (g_modules[i].type == RP_MOD_RPA && g_modules[i].as.rpa->can_handle &&
            g_modules[i].as.rpa->can_handle(hdr, len))
            return g_modules[i].as.rpa;
    }
    return NULL;
}

const rpa_handler_t *rpa_handler_for_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    uint8_t hdr[64];
    size_t n = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    return rpa_handler_for_data(hdr, n);
}

const rpa_handler_t *rpa_handler_get(const char *id) {
    plugin_init();
    const rp_module_t *m = find_mod(RP_MOD_RPA, id);
    return m ? m->as.rpa : NULL;
}

const mod_module_t *mod_module_get(const char *id) {
    plugin_init();
    const rp_module_t *m = find_mod(RP_MOD_MOD, id);
    return m ? m->as.mod : NULL;
}

const inject_module_t *inject_module_get(const char *id) {
    plugin_init();
    const rp_module_t *m = find_mod(RP_MOD_INJECT, id);
    return m ? m->as.inject : NULL;
}

const translator_backend_t *translator_get(const char *id) {
    plugin_init();
    const rp_module_t *m = find_mod(RP_MOD_TRANSLATOR, id);
    return m ? m->as.translator : NULL;
}

const exporter_t *exporter_get(const char *id) {
    plugin_init();
    const rp_module_t *m = find_mod(RP_MOD_EXPORTER, id);
    return m ? m->as.exporter : NULL;
}
