/* core/game_mgr.c —— 见 core/game_mgr.h
 *
 * 本地游戏登记表：把常用游戏的 .rpa 路径登记下来，之后用别名即可调用。
 * 存储格式为纯文本，每行一条：
 *     name|path|added_unix|note
 * 不引入 JSON 依赖，保持零外部库。
 */
#include "core/game_mgr.h"
#include "core/plugin.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

#define MAX_GAMES 256
#define LINE_MAX  2048

static game_entry_t g_games[MAX_GAMES];
static int          g_ngames = 0;
static char         g_store[2048] = "games.txt";

/* ---- 目录创建：仅创建 path 的父目录（不创建最后的文件名组件） ---- */
static void mkdirs(const char *path) {
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *last = strrchr(tmp, '/');
    if (!last || last == tmp) return;   /* 无父目录或根目录 */
    *last = '\0';                      /* 现在 tmp 是父目录 */
    for (char *p = tmp + 1; *p; p++) {
#ifndef _WIN32
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
#else
        if (*p == '/' || *p == '\\') { *p = '\0'; _mkdir(tmp); *p = '/'; }
#endif
    }
#ifndef _WIN32
    mkdir(tmp, 0755);
#else
    _mkdir(tmp);
#endif
}

static const char *default_store(void) {
    const char *home = getenv("RENPY_PATCHER_HOME");
    if (!home) home = getenv("HOME");
#ifdef _WIN32
    if (!home) home = getenv("APPDATA");
#endif
    if (home) {
        static char buf[2048];
        snprintf(buf, sizeof(buf), "%s/renpy_patcher/games.txt", home);
        return buf;
    }
    return "games.txt";
}

void game_mgr_init(const char *store_path) {
    snprintf(g_store, sizeof(g_store), "%s", store_path ? store_path : default_store());
    g_ngames = 0;

    FILE *f = fopen(g_store, "r");
    if (!f) { log_debug("游戏登记表不存在: %s（将新建）", g_store); return; }
    char line[LINE_MAX];
    while (fgets(line, sizeof(line), f) && g_ngames < MAX_GAMES) {
        /* 去掉换行 */
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (L == 0) continue;

        game_entry_t *g = &g_games[g_ngames];
        memset(g, 0, sizeof(*g));
        /* name | path | added | note （note 可含 | 不保证，简单截断） */
        char *p = line;
        char *f1 = strchr(p, '|'); if (!f1) continue;
        *f1 = '\0'; snprintf(g->name, sizeof(g->name), "%s", p); p = f1 + 1;
        char *f2 = strchr(p, '|'); if (!f2) continue;
        *f2 = '\0'; snprintf(g->path, sizeof(g->path), "%s", p); p = f2 + 1;
        char *f3 = strchr(p, '|');
        if (f3) {
            *f3 = '\0'; g->added = (int64_t)strtoll(p, NULL, 10); p = f3 + 1;
            snprintf(g->note, sizeof(g->note), "%s", p);
        } else {
            g->added = (int64_t)strtoll(p, NULL, 10);
        }
        g_ngames++;
    }
    fclose(f);
    log_info("已加载游戏登记表 %s（%d 条）", g_store, g_ngames);
}

static int save(void) {
    mkdirs(g_store);
    FILE *f = fopen(g_store, "w");
    if (!f) { log_error("无法写入游戏登记表: %s", g_store); return RP_ERR_IO; }
    for (int i = 0; i < g_ngames; i++) {
        game_entry_t *g = &g_games[i];
        fprintf(f, "%s|%s|%lld|%s\n", g->name, g->path, (long long)g->added, g->note);
    }
    fclose(f);
    log_debug("已保存游戏登记表（%d 条）", g_ngames);
    return RP_OK;
}

int game_add(const char *name, const char *path, const char *note) {
    if (!name || !*name || !path || !*path) return RP_ERR_GENERIC;
    /* 同名覆盖 */
    for (int i = 0; i < g_ngames; i++) {
        if (strcmp(g_games[i].name, name) == 0) {
            snprintf(g_games[i].path, sizeof(g_games[i].path), "%s", path);
            snprintf(g_games[i].note, sizeof(g_games[i].note), "%s", note ? note : "");
            g_games[i].added = (int64_t)time(NULL);
            save();
            return RP_OK;
        }
    }
    if (g_ngames >= MAX_GAMES) { log_error("游戏数量已达上限 %d", MAX_GAMES); return RP_ERR_NOMEM; }
    game_entry_t *g = &g_games[g_ngames++];
    snprintf(g->name, sizeof(g->name), "%s", name);
    snprintf(g->path, sizeof(g->path), "%s", path);
    snprintf(g->note, sizeof(g->note), "%s", note ? note : "");
    g->added = (int64_t)time(NULL);
    save();
    return RP_OK;
}

int game_remove(const char *name) {
    for (int i = 0; i < g_ngames; i++) {
        if (strcmp(g_games[i].name, name) == 0) {
            for (int j = i; j < g_ngames - 1; j++) g_games[j] = g_games[j + 1];
            g_ngames--;
            save();
            return RP_OK;
        }
    }
    return RP_ERR_NOTSUP;
}

const game_entry_t *game_get(const char *name) {
    for (int i = 0; i < g_ngames; i++)
        if (strcmp(g_games[i].name, name) == 0) return &g_games[i];
    return NULL;
}

size_t game_count(void) { return (size_t)g_ngames; }

size_t game_list(int (*cb)(const game_entry_t *, void *), void *udata) {
    size_t n = 0;
    for (int i = 0; i < g_ngames; i++) {
        n++;
        if (cb && cb(&g_games[i], udata) != 0) break;
    }
    return n;
}

const char *game_resolve(const char *token) {
    static char buf[1024];
    const game_entry_t *g = game_get(token);
    if (g) { snprintf(buf, sizeof(buf), "%s", g->path); return buf; }
    return token;   /* 不是登记名，原样当作路径 */
}
