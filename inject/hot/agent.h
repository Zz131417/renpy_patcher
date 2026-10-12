/* inject/hot/agent.h —— 热注入模块契约（被注入的 agent 与注入器共用） */
#ifndef RENPY_PATCHER_HOT_AGENT_H
#define RENPY_PATCHER_HOT_AGENT_H

#include "core/plugin.h"

#ifdef _WIN32
#include <windows.h>

/* 注入器与 agent 通过命名内存映射传递 mod 路径 */
#define RP_HOT_MAP_NAME   "Local\\RenpyPatcherModConfig"
#define RP_HOT_DONE_EVENT "Local\\RenpyPatcherDone"

/* 注入器在注入 agent.dll 前，把 mod 目录绝对路径写入此结构所在映射 */
typedef struct hot_config {
    char mod_path[MAX_PATH];   /* mod 目录（含 mod_main.py） */
} hot_config_t;

#endif /* _WIN32 */

#ifdef __cplusplus
extern "C" {
#endif

/* 热注入入口：向 pid 进程注入 agent 并加载 mod。
 * 非 Windows 平台返回 RP_ERR_NOTSUP。 */
int hot_inject(int pid, const char *mod_path);

/* 注册的注入模块实例（供 plugin_loader 注册） */
extern const inject_module_t g_inject_hot;

#ifdef __cplusplus
}
#endif
#endif
