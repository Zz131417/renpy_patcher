/* util/sfx.c —— 见 util/sfx.h
 *
 * 生成自解压 exe（跨平台拼接逻辑）：
 *   stub_exe + payload.zip + [ "RPEX" ][ stub_size(uint64 LE) ]
 * stub 运行时读到自身尾部 "RPEX" 哨兵，得到 stub_size（zip 起始偏移），
 * 把内置 zip 取出后解压（复用 util/zip_extract）。
 *
 * 仅负责「拼接」，不含任何 Windows 专属逻辑；找不到 stub 时返回 RP_ERR_NOTSUP，
 * 由调用方提示「仅 Windows 支持生成 exe」。
 */
#include "util/sfx.h"
#include "util/zip.h"
#include "core/plugin.h"
#include "core/plugin_loader.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_file_bytes(const char *path, unsigned char **out, size_t *outlen)
{
    FILE *f = fopen(path, "rb");
    if (!f) return RP_ERR_IO;
    fseek(f, 0, SEEK_END); long sz = ftell(f);
    if (sz < 0) { fclose(f); return RP_ERR_IO; }
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return RP_ERR_NOMEM; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *out = buf; *outlen = rd;
    return RP_OK;
}

static int write_file_bytes(const char *path, const unsigned char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return RP_ERR_IO;
    size_t wr = fwrite(data, 1, len, f);
    fclose(f);
    return (wr == len) ? RP_OK : RP_ERR_IO;
}

/* 在 src_dir 打包成临时 zip；autorun_cmd 非空则注入 RP_AUTORUN.txt */
static int make_payload(const char *src_dir, const char *tmp_zip,
                        const char *autorun_cmd, int compress)
{
    zip_writer_t *z = zip_open(tmp_zip, 0);
    if (!z) return RP_ERR_IO;
    int rc = zip_add_dir(z, src_dir, "", compress);
    if (rc == RP_OK && autorun_cmd && *autorun_cmd) {
        rc = zip_add_file_data(z, "RP_AUTORUN.txt", autorun_cmd, strlen(autorun_cmd), 0);
    }
    if (rc == RP_OK) rc = zip_close(z);
    else zip_close(z);
    return rc;
}

int sfx_build(const char *stub_path, const char *src_dir,
              const char *out_exe, const char *autorun_cmd, int compress)
{
    if (!stub_path || !*stub_path) return RP_ERR_NOTSUP;
    FILE *t = fopen(stub_path, "rb");
    if (!t) { log_warn("自解压桩缺失：%s（仅 Windows 支持生成 exe）", stub_path); return RP_ERR_NOTSUP; }
    fclose(t);

    /* 临时 zip：与 out_exe 同目录，避免跨盘 */
    char tmp_zip[2048];
    snprintf(tmp_zip, sizeof(tmp_zip), "%s.payload.zip", out_exe);
    int rc = make_payload(src_dir, tmp_zip, autorun_cmd, compress);
    if (rc != RP_OK) return rc;

    unsigned char *stub = NULL, *payload = NULL; size_t slen = 0, plen = 0;
    rc = read_file_bytes(stub_path, &stub, &slen);
    if (rc == RP_OK) rc = read_file_bytes(tmp_zip, &payload, &plen);
    if (rc == RP_OK) {
        size_t total = slen + plen + 4 + 8;
        unsigned char *buf = malloc(total + 1);
        if (!buf) rc = RP_ERR_NOMEM;
        else {
            memcpy(buf, stub, slen);
            memcpy(buf + slen, payload, plen);
            memcpy(buf + slen + plen, "RPEX", 4);
            unsigned long long ss = (unsigned long long)slen;
            for (int i = 0; i < 8; i++)
                buf[slen + plen + 4 + i] = (unsigned char)((ss >> (8 * i)) & 0xff);
            rc = write_file_bytes(out_exe, buf, total);
            free(buf);
        }
    }
    if (stub) free(stub);
    if (payload) free(payload);
    remove(tmp_zip);
    return rc;
}

int sfx_build_from_zip(const char *stub_path, const char *zip_path,
                       const char *out_exe, const char *autorun_cmd)
{
    if (!stub_path || !*stub_path) return RP_ERR_NOTSUP;
    FILE *t = fopen(stub_path, "rb");
    if (!t) { log_warn("自解压桩缺失：%s（仅 Windows 支持生成 exe）", stub_path); return RP_ERR_NOTSUP; }
    fclose(t);
    /* 注：autorun_cmd 对已有 zip 不注入（保持原 zip 不变）；启动器请改用 sfx_build 传目录。 */
    (void)autorun_cmd;

    unsigned char *stub = NULL, *payload = NULL; size_t slen = 0, plen = 0;
    int rc = read_file_bytes(stub_path, &stub, &slen);
    if (rc == RP_OK) rc = read_file_bytes(zip_path, &payload, &plen);
    if (rc == RP_OK) {
        size_t total = slen + plen + 4 + 8;
        unsigned char *buf = malloc(total + 1);
        if (!buf) rc = RP_ERR_NOMEM;
        else {
            memcpy(buf, stub, slen);
            memcpy(buf + slen, payload, plen);
            memcpy(buf + slen + plen, "RPEX", 4);
            unsigned long long ss = (unsigned long long)slen;
            for (int i = 0; i < 8; i++)
                buf[slen + plen + 4 + i] = (unsigned char)((ss >> (8 * i)) & 0xff);
            rc = write_file_bytes(out_exe, buf, total);
            free(buf);
        }
    }
    if (stub) free(stub);
    if (payload) free(payload);
    return rc;
}
