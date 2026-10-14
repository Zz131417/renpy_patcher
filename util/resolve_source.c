/* util/resolve_source.c —— 见 util/resolve_source.h
 *
 * 把「游戏来源」归一化：.rpa / 别名直接返回；.zip 整包先解压再定位内部 .rpa。
 * 复用 util/zip_extract（解压）与 util/dir_walk（找文件），跨平台。
 */
#include "util/resolve_source.h"
#include "util/zip_extract.h"
#include "util/dir_walk.h"
#include "core/plugin.h"
#include "core/plugin_loader.h"
#include "core/game_mgr.h"
#include "util/log.h"
#include <stdio.h>
#include <string.h>

int resolve_game_source(const char *input, const char *out_dir,
                        char *out_rpa, size_t rpa_n)
{
    out_rpa[0] = '\0';
    if (!input || !*input) return RP_ERR_GENERIC;

    const char *resolved = game_resolve(input);   /* 支持别名 */
    if (!is_zip_file(resolved)) {
        /* 非 zip：当作 .rpa 路径直接使用 */
        return RP_OK;
    }

    /* 是 zip：解压到 out_dir，再找第一个 .rpa */
    int cnt = 0;
    int rc = zip_extract(resolved, out_dir, &cnt);
    if (rc != RP_OK) { log_error("解压游戏压缩包失败，错误码 %d", rc); return rc; }

    file_entry_t *files = NULL; size_t n = 0;
    if (collect_files(out_dir, &files, &n) == RP_OK) {
        for (size_t i = 0; i < n; i++) {
            const char *p = files[i].full_path;
            size_t L = strlen(p);
            if (L > 4 && strcmp(p + L - 4, ".rpa") == 0) {
                snprintf(out_rpa, rpa_n, "%s", p);
                break;
            }
        }
        free_files(files, n);
    }
    if (out_rpa[0] == '\0') {
        log_error("压缩包内未找到 .rpa 资源包（解压出 %d 个文件）", cnt);
        return RP_ERR_FORMAT;
    }
    return RP_OK;
}
