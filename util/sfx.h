#ifndef RP_SFX_H
#define RP_SFX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 生成一个「自解压安装包 exe」：将 stub（Windows 原生自解压桩）与目录打成的内置 zip
 * 拼接成一个 .exe。运行该 exe 时，stub 会解出内置 zip 并（可选）自动运行 autorun_cmd。
 *
 *  - stub_path   : 自解压桩可执行文件（Windows 原生 exe）。找不到时返回 RP_ERR_NOTSUP。
 *  - src_dir     : 要打包的目录（游戏 / mod）。
 *  - out_exe     : 输出 exe 路径。
 *  - autorun_cmd : 非空时写入内置 RP_AUTORUN.txt，stub 解压后自动执行该命令（用于「启动器」）。
 *                  NULL 则为纯解压安装包。
 *  - compress    : 非 0 用 deflate 压缩。
 * 返回 RP_OK 成功，否则错误码。
 */
int sfx_build(const char *stub_path, const char *src_dir,
              const char *out_exe, const char *autorun_cmd, int compress);

/* 已有 zip 的情况：直接拼接（不注入 autorun；如需启动器请用 sfx_build 传目录）。 */
int sfx_build_from_zip(const char *stub_path, const char *zip_path,
                       const char *out_exe, const char *autorun_cmd);

#ifdef __cplusplus
}
#endif
#endif /* RP_SFX_H */
