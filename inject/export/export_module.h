#ifndef RP_EXPORT_MODULE_H
#define RP_EXPORT_MODULE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CLI 命令（解析 argc/argv）。见 export_module.c。 */
int export_run(int argc, char **argv);

/* 简单参数版（GUI / 其他前端调用）：
 *   dir        要打包的目录
 *   out        输出路径（建议 .zip）
 *   split_size 分卷大小（字节），0 表示单文件
 *   compress   1=deflate 压缩，0=仅存储
 * 返回 RP_OK 或错误码。 */
int export_dir(const char *dir, const char *out, size_t split_size, int compress);

#ifdef __cplusplus
}
#endif

#endif /* RP_EXPORT_MODULE_H */
