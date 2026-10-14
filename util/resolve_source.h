#ifndef RP_RESOLVE_SOURCE_H
#define RP_RESOLVE_SOURCE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 把「游戏来源」归一化为可直接喂给 unpack/build 的 .rpa 路径：
 *   - 若输入是 .rpa / 别名，原样返回（out_rpa 置空，返回 RP_OK）。
 *   - 若输入是 .zip（整包游戏），先解压到 out_dir（调用方提供），
 *     再在其中查找第一个 .rpa，写入 out_rpa（含路径）。
 * 返回 RP_OK 成功；找不到 rpa 时 out_rpa[0]='\0' 且返回 RP_ERR_FORMAT。
 */
int resolve_game_source(const char *input, const char *out_dir,
                        char *out_rpa, size_t rpa_n);

#ifdef __cplusplus
}
#endif
#endif /* RP_RESOLVE_SOURCE_H */
