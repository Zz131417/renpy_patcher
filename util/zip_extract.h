#ifndef RP_ZIP_EXTRACT_H
#define RP_ZIP_EXTRACT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 解压一个 .zip 到 out_dir（自动创建子目录）。
 * 支持 store(0) 与 deflate(8)。out_count 返回解出的文件数（可为 NULL）。
 * zip_path 指向单文件 .zip；若为分卷（.z01...zip），传 .zip 路径即可自动拼接。
 * 返回 RP_OK 成功，否则错误码。 */
int zip_extract(const char *zip_path, const char *out_dir, int *out_count);

/* 判断文件是否为合法 zip（含分卷的 .z01 / .zip）。供调用方区分「游戏是 zip 还是 rpa」。 */
int is_zip_file(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* RP_ZIP_EXTRACT_H */
