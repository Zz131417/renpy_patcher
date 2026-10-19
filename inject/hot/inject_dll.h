/* inject/hot/inject_dll.h —— 通用 DLL 注入原语（任意 DLL，不依赖 agent/Python）
 *
 * 与 hot_inject（注入 agent.dll 并加载 mod）不同，这里注入的是用户给定的任意 DLL。
 * 仅供 GUI / CLI 调用，做「进程级 DLL 注入」。
 */
#ifndef RENPY_PATCHER_HOT_INJECT_DLL_H
#define RENPY_PATCHER_HOT_INJECT_DLL_H

#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 经典 LoadLibrary 远程线程注入：把 dll_path 注入 pid 进程（Windows）。
 * 非 Windows 平台返回 RP_ERR_NOTSUP。 */
int inject_dll_into_pid(int pid, const char *dll_path);

/* 启动 exe_path，把 dll_path 注入新进程：
 *   delay_ms <= 0：创建挂起进程 → 注入 → 恢复（最稳，目标尚未初始化）
 *   delay_ms  > 0：正常启动 → 等待 delay_ms 毫秒 → 注入
 * 非 Windows 返回 RP_ERR_NOTSUP。 */
int inject_dll_into_exe(const char *exe_path, const char *dll_path, int delay_ms);

#ifdef __cplusplus
}
#endif
#endif /* RENPY_PATCHER_HOT_INJECT_DLL_H */
