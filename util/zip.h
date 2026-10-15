#ifndef RP_ZIP_H
#define RP_ZIP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct zip_writer zip_writer_t;

/* 打开 zip 写入器。
 * out_path: 输出路径（带不带 .zip 均可）；split_size>0 时分卷，
 *           每卷 <= split_size 字节（字节级分卷，按 cat 重组即可还原）。
 * 返回 NULL 表示失败。 */
zip_writer_t *zip_open(const char *out_path, size_t split_size);

/* 添加一个内存缓冲区作为文件（arc_name 为包内路径，用 '/' 分隔）。 */
int zip_add_file_data(zip_writer_t *z, const char *arc_name,
                      const void *data, size_t len, int compress);

/* 从磁盘文件添加（自动读取内容）。 */
int zip_add_file(zip_writer_t *z, const char *arc_name,
                 const char *disk_path, int compress);

/* 递归添加整个目录；arc_root 为包内根前缀（"" 表示不加前缀）。 */
int zip_add_dir(zip_writer_t *z, const char *dir_path,
                const char *arc_root, int compress);

/* 关闭并写出（分卷时生成 .z01 ... .zip）。返回 RP_OK。 */
int zip_close(zip_writer_t *z);

/* 设置条目数据对齐字节数（APK 需要 4 字节对齐，对应 zipalign 4）。
 * align<=1 表示不填充；其余按 (align - len%align) 在每条数据前补零。 */
int zip_set_alignment(zip_writer_t *z, uint16_t align);

#ifdef __cplusplus
}
#endif
#endif /* RP_ZIP_H */
