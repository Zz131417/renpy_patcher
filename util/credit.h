#ifndef RP_CREDIT_H
#define RP_CREDIT_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* 写入 rpa 包内的声明文件名（ZZ_ 前缀降低与游戏文件冲突概率） */
extern const char *CREDIT_FILENAME;

/* 生成「由 Ren'Py Patcher 注入/汉化」的声明文字到 buf（n 为容量）。
 * game_name 可为 NULL（显示为 unknown）。返回 RP_OK。 */
int credit_generate_text(char *buf, size_t n, const char *game_name);

/* 把声明文件写入目录 dir（直接注入时在合并目录里调用）。返回 RP_OK。 */
int credit_write_file(const char *dir, const char *game_name);

/* 给已存在的 rpa 追加声明：解包 -> 写声明 -> 重打包。
 * in_rpa / out_rpa 可相同（内部先处理再覆盖）。返回 RP_OK。 */
int credit_add_to_rpa(const char *in_rpa, const char *out_rpa, const char *game_name);

#ifdef __cplusplus
}
#endif
#endif /* RP_CREDIT_H */
