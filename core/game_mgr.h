#ifndef RP_GAME_MGR_H
#define RP_GAME_MGR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 单个已登记游戏的信息 */
typedef struct {
    char    name[128];   /* 游戏别名（主键，唯一） */
    char    path[1024];  /* .rpa 文件或游戏目录的绝对/相对路径 */
    char    note[256];   /* 备注（版本、来源等，可选） */
    int64_t added;       /* 登记时间（Unix 秒） */
} game_entry_t;

/* 初始化游戏管理。store_path 为 NULL 时使用默认位置：
 *   Linux/macOS: $RENPY_PATCHER_HOME 或 $HOME/.renpy_patcher/games.txt
 *   Windows:     %RENPY_PATCHER_HOME% 或 %APPDATA%/renpy_patcher/games.txt
 * 调用其他函数前需先调用一次。 */
void game_mgr_init(const char *store_path);

/* 添加或覆盖一个游戏（同名则覆盖）。返回 RP_OK。 */
int  game_add(const char *name, const char *path, const char *note);
/* 删除一个游戏。存在并删除返回 RP_OK，不存在返回 RP_ERR_NOTSUP。 */
int  game_remove(const char *name);
/* 取游戏信息，不存在返回 NULL（返回指针在下次调用前有效）。 */
const game_entry_t *game_get(const char *name);
/* 游戏数量。 */
size_t game_count(void);
/* 遍历所有游戏：对每条调用 cb；cb 返回非 0 提前结束。返回遍历条数。 */
size_t game_list(int (*cb)(const game_entry_t *, void *udata), void *udata);

/* 解析一个输入 token：
 *   - 若 token 匹配已登记游戏名，返回其 path（内部静态缓冲）；
 *   - 否则原样返回 token（即直接当作路径使用）。
 * 用于让 unpack/build 等命令既能吃路径也能吃游戏别名。 */
const char *game_resolve(const char *token);

#ifdef __cplusplus
}
#endif
#endif /* RP_GAME_MGR_H */
