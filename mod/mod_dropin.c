/* mod/mod_dropin.c —— drop-in 模式实现。
 * 在已解包游戏的 game/ 目录下生成 mod/ 与 dll/ 子目录，并写入一个 RenPy bootstrap
 *（zzz_renpy_patcher.rpy）。游戏原 exe 启动时，RenPy 自动执行该 bootstrap：
 *   - 用 ctypes 把 game/dll/ 下每个 .dll 加载进游戏进程（同进程注入）
 *   - 若存在 game/mod/mod_main.py 则执行它（运行时 Python 钩子）
 * 而 game/mod/ 下的普通资源/脚本（.rpy/.png 等）由 RenPy 自身递归扫描自动加载，
 * 因此「文件级 mod」与「mod_main.py 钩子」两者都支持，且无需改动游戏本体。 */
#include "mod/mod_dropin.h"
#include "util/dir_walk.h"
#include "util/log.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SEP '\\'
#define STRCASECMP _stricmp
#else
#include <sys/stat.h>
#include <sys/types.h>
#define SEP '/'
#define STRCASECMP strcasecmp
#endif

/* RenPy bootstrap 内容（自动生成，勿手改）。
 * 用 renpy.config.gamedir 定位 game 根；init -1000 早加载 dll，init 1000 晚执行 mod_main.py。 */
static const char *RP_BOOTSTRAP =
"# zzz_renpy_patcher.rpy —— RenPy Patcher 自动生成，请勿手改\n"
"# 游戏启动时自动加载同目录的 mod/ 与 dll/\n"
"init -1000 python:\n"
"    import os\n"
"    import ctypes\n"
"    import renpy\n"
"    _rp_gdir = renpy.config.gamedir\n"
"    _rp_dll_dir = os.path.join(_rp_gdir, \"dll\")\n"
"    if os.path.isdir(_rp_dll_dir):\n"
"        for _rp_f in sorted(os.listdir(_rp_dll_dir)):\n"
"            if _rp_f.lower().endswith(\".dll\"):\n"
"                try:\n"
"                    ctypes.CDLL(os.path.join(_rp_dll_dir, _rp_f))\n"
"                except Exception:\n"
"                    pass\n"
"init 1000 python:\n"
"    import os\n"
"    import renpy\n"
"    _rp_gdir = renpy.config.gamedir\n"
"    _rp_mod_main = os.path.join(_rp_gdir, \"mod\", \"mod_main.py\")\n"
"    if os.path.isfile(_rp_mod_main):\n"
"        try:\n"
"            with open(_rp_mod_main, \"r\", encoding=\"utf-8\") as _rp_fh:\n"
"                _rp_code = _rp_fh.read()\n"
"            exec(compile(_rp_code, _rp_mod_main, \"exec\"), globals())\n"
"        except Exception:\n"
"            pass\n";

/* 逐级创建目录（跨平台，已存在则忽略）。 */
static int ensure_dir(const char *path)
{
#ifdef _WIN32
    char buf[2048];
    int L = snprintf(buf, sizeof(buf), "%s", path);
    if (L < 0 || L >= (int)sizeof(buf)) return RP_ERR_IO;
    for (int i = 1; i < L; i++) {
        if (buf[i] == SEP) { char t = buf[i]; buf[i] = 0; CreateDirectoryA(buf, NULL); buf[i] = t; }
    }
    CreateDirectoryA(buf, NULL);
#else
    char buf[2048];
    int L = snprintf(buf, sizeof(buf), "%s", path);
    if (L < 0 || L >= (int)sizeof(buf)) return RP_ERR_IO;
    for (int i = 1; i < L; i++) {
        if (buf[i] == SEP) { char t = buf[i]; buf[i] = 0; mkdir(buf, 0755); buf[i] = t; }
    }
    mkdir(buf, 0755);
#endif
    return RP_OK;
}

/* 取父目录并创建（用于保证 dst 文件可写入）。 */
static int ensure_parent(const char *path)
{
    char buf[2048];
    int L = snprintf(buf, sizeof(buf), "%s", path);
    if (L < 0 || L >= (int)sizeof(buf)) return RP_ERR_IO;
    for (int i = L - 1; i >= 0; i--) {
        if (buf[i] == SEP) { buf[i] = 0; return ensure_dir(buf); }
    }
    return RP_OK;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *fi = fopen(src, "rb");
    if (!fi) { rp_set_error("无法打开源文件: %s", src); return RP_ERR_IO; }
    ensure_parent(dst);
    FILE *fo = fopen(dst, "wb");
    if (!fo) { fclose(fi); rp_set_error("无法写入目标文件: %s", dst); return RP_ERR_IO; }
    char buf[65536]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        if (fwrite(buf, 1, n, fo) != n) { fclose(fi); fclose(fo); return RP_ERR_IO; }
    }
    fclose(fi); fclose(fo);
    return RP_OK;
}

static void join_path(char *out, size_t n, const char *a, const char *b)
{
    int L = snprintf(out, n, "%s", a);
    if (L > 0 && L < (int)n && out[L - 1] != SEP) { out[L] = SEP; out[L + 1] = 0; }
    strncat(out, b, n - strlen(out) - 1);
}

/* 返回 rel_path 的 basename（最后一段）。 */
static const char *basename_of(const char *rel)
{
    const char *p = rel + strlen(rel);
    while (p > rel && p[-1] != '/' && p[-1] != SEP) p--;
    return p;
}

int mod_dropin_install(const char *game_dir, const char *mod_src, const char *dll_dir)
{
    if (!game_dir || !*game_dir) { rp_set_error("未提供已解包目录"); return RP_ERR_NOTSUP; }

    /* 资源根：优先 game_dir/game，否则直接用 game_dir */
    char game_root[2048];
    char probe[2048];
    join_path(probe, sizeof(probe), game_dir, "game");
    int has_game = 0;
#ifdef _WIN32
    has_game = (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES);
#else
    struct stat st; has_game = (stat(probe, &st) == 0 && S_ISDIR(st.st_mode));
#endif
    join_path(game_root, sizeof(game_root), has_game ? probe : game_dir, "");

    /* 1) 写 bootstrap（始终生成） */
    char boot[2048];
    join_path(boot, sizeof(boot), game_root, "zzz_renpy_patcher.rpy");
    ensure_parent(boot);
    FILE *bf = fopen(boot, "wb");
    if (!bf) { rp_set_error("无法写入 bootstrap: %s", boot); return RP_ERR_IO; }
    fwrite(RP_BOOTSTRAP, 1, strlen(RP_BOOTSTRAP), bf);
    fclose(bf);
    log_info("drop-in: 已写入 bootstrap %s", boot);

    /* 2) 复制 mod_src → game_root/mod/ */
    if (mod_src && *mod_src) {
        char moddst[2048];
        join_path(moddst, sizeof(moddst), game_root, "mod");
        ensure_dir(moddst);
        file_entry_t *files = NULL; size_t n = 0;
        if (collect_files(mod_src, &files, &n) == RP_OK) {
            char dst[2048];
            for (size_t i = 0; i < n; i++) {
                join_path(dst, sizeof(dst), moddst, files[i].rel_path);
                if (copy_file(files[i].full_path, dst) != RP_OK) {
                    log_error("drop-in: 复制 mod 文件失败 %s", files[i].full_path);
                }
            }
            free_files(files, n);
        }
        log_info("drop-in: mod 已装入 %s", moddst);
    }

    /* 3) 复制 dll_dir/*.dll → game_root/dll/ */
    if (dll_dir && *dll_dir) {
        char dlldst[2048];
        join_path(dlldst, sizeof(dlldst), game_root, "dll");
        ensure_dir(dlldst);
        file_entry_t *files = NULL; size_t n = 0;
        if (collect_files(dll_dir, &files, &n) == RP_OK) {
            char dst[2048];
            for (size_t i = 0; i < n; i++) {
                const char *bn = basename_of(files[i].rel_path);
                size_t bl = strlen(bn);
                if (bl >= 4 && STRCASECMP(bn + bl - 4, ".dll") == 0) {
                    join_path(dst, sizeof(dst), dlldst, bn);
                    if (copy_file(files[i].full_path, dst) != RP_OK) {
                        log_error("drop-in: 复制 dll 失败 %s", files[i].full_path);
                    }
                }
            }
            free_files(files, n);
        }
        log_info("drop-in: dll 已装入 %s", dlldst);
    }

    return RP_OK;
}
