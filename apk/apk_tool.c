/* apk/apk_tool.c —— 基于已有 APK 重打包（注入 mod，生成未签名 APK）
 *
 * 见 apk/apk_tool.h。核心三步：zip_extract 解开 → merge_mod_into_dir 注入
 * → 用带 4 字节对齐的 zip 写入器重打包（默认去掉旧签名，等你用 apksigner 签名）。
 */
#include "apk/apk_tool.h"
#include "core/plugin.h"
#include "core/plugin_loader.h"
#include "util/log.h"
#include "util/zip.h"
#include "util/zip_extract.h"
#include "util/dir_walk.h"
#include "mod/mod_manifest.h"   /* merge_mod_into_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

/* 创建一次性临时目录 */
static int make_tmp_dir(char *buf, size_t n) {
#ifdef _WIN32
    char base[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, base)) return -1;
    snprintf(buf, n, "%srenpy_apk_%u", base, GetCurrentProcessId());
    return CreateDirectoryA(buf, NULL) ? 0 : -1;
#else
    snprintf(buf, n, "/tmp/renpy_apk_%d", (int)getpid());
    return mkdir(buf, 0700);
#endif
}

static int is_old_sig(const char *arc) {
    /* META-INF 下的签名文件：.SF / .RSA / .DSA / .EC（v1/v2 签名遗物） */
    if (strncmp(arc, "META-INF/", 9) != 0) return 0;
    const char *dot = strrchr(arc, '.');
    if (!dot) return 0;
    return (strcmp(dot, ".SF") == 0 || strcmp(dot, ".RSA") == 0 ||
            strcmp(dot, ".DSA") == 0 || strcmp(dot, ".EC") == 0);
}

int apk_repack(const char *apk_path, const char *mod_dir, const char *out_apk, int strip_sig) {
    if (!apk_path || !out_apk || !*apk_path || !*out_apk)
        return RP_ERR_GENERIC;

    char tmp[2048];
    if (make_tmp_dir(tmp, sizeof(tmp)) != 0) {
        log_error("无法创建临时目录用于解包 APK");
        return RP_ERR_IO;
    }

    int rc = zip_extract(apk_path, tmp, NULL);
    if (rc != RP_OK) { log_error("APK 解压失败: %s", apk_path); return rc; }
    log_info("APK 已解包到临时目录");

    if (mod_dir && mod_dir[0]) {
        rc = merge_mod_into_dir(tmp, mod_dir);
        if (rc != RP_OK) { log_error("mod 合并失败"); return rc; }
        log_info("已注入 mod: %s", mod_dir);
    }

    zip_writer_t *z = zip_open(out_apk, 0);
    if (!z) { log_error("无法打开输出 APK: %s", out_apk); return RP_ERR_IO; }
    zip_set_alignment(z, 4);   /* APK 要求 4 字节对齐（zipalign 4） */

    file_entry_t *files = NULL; size_t n = 0;
    rc = collect_files(tmp, &files, &n);
    if (rc != RP_OK) { zip_close(z); return rc; }

    size_t added = 0;
    for (size_t i = 0; i < n; i++) {
        char arc[2048];
        snprintf(arc, sizeof(arc), "%s", files[i].rel_path);
        for (char *p = arc; *p; p++) if (*p == '\\') *p = '/';
        if (strip_sig && is_old_sig(arc)) {
            log_info("移除旧签名: %s", arc);
            continue;
        }
        if (zip_add_file(z, arc, files[i].full_path, 1) == RP_OK) added++;
        else log_warn("跳过打包失败的文件: %s", arc);
    }
    free_files(files, n);

    rc = zip_close(z);
    if (rc == RP_OK) {
        log_info("APK 重打包完成: %s（共 %zu 条目）%s",
                 out_apk, added,
                 strip_sig ? " [已移除旧签名，请用 apksigner 重新签名]" : "");
        if (strip_sig)
            log_info("签名示例: apksigner sign --ks my.keystore %s", out_apk);
    }
    return rc;
}
