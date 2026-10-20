/* mod/mod_dropin.h —— drop-in 模式：把 mod 装进已解包游戏的 game/ 下，
 * 游戏原 exe 启动时自动读取同目录的 mod/ 与 dll/。 */
#ifndef RENPY_PATCHER_MOD_DROPIN_H
#define RENPY_PATCHER_MOD_DROPIN_H

#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 把 mod 以 drop-in 方式装入已解包目录的 game/ 下：
 *   - 在 game/<root>/mod/  放入来自 mod_src 的文件（资源/脚本由 RenPy 自动发现，mod_main.py 由 bootstrap 执行）
 *   - 在 game/<root>/dll/  放入来自 dll_dir 的 *.dll（由 bootstrap 用 ctypes 加载进游戏进程）
 *   - 写入 bootstrap：game/<root>/zzz_renpy_patcher.rpy（RenPy 启动时自动运行）
 * mod_src / dll_dir 可为 NULL 或空串（仅写 bootstrap）。
 * game_dir 应指向「已解包目录」（其下含 game/）；若无 game/ 子目录则直接以 game_dir 为资源根。
 * 返回 RP_OK / 错误码。 */
int mod_dropin_install(const char *game_dir, const char *mod_src, const char *dll_dir);

#ifdef __cplusplus
}
#endif
#endif
