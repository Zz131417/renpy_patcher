/* util/zip_extract.c —— 见 util/zip_extract.h
 *
 * 极简 ZIP 读取/解压（PKWARE APPNOTE 规范子集，与本项目 zip.c 写法对应）：
 *   - 读取 EOCD -> 中央目录 -> 各 local header
 *   - 支持 store(0) 与 deflate(8)（deflate 经 zlib 的裸 deflate 流）
 *   - 用 zlib 的 crc32 校验
 *   - 支持分卷：.z01 ... .zNN + .zip，按序号拼接成完整字节流后解析
 *
 * 仅依赖 zlib，零第三方库，与项目一致。
 */
#include "util/zip_extract.h"
#include "core/plugin.h"
#include "core/plugin_loader.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define SIG_LOCAL   0x04034b50u
#define SIG_CD      0x02014b50u
#define SIG_EOCD    0x06054b50u

/* 读整文件到内存 */
static int read_all(const char *path, unsigned char **out, size_t *outlen)
{
    FILE *f = fopen(path, "rb");
    if (!f) return RP_ERR_IO;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return RP_ERR_IO; }
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return RP_ERR_NOMEM; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *out = buf; *outlen = rd;
    return RP_OK;
}

/* 把分卷（.z01...zip）按序拼成单文件；若非分卷直接读 .zip。
 * 返回内存块（调用方 free）。 */
static int load_zip_stream(const char *zip_path, unsigned char **out, size_t *outlen)
{
    /* 是否分卷：存在同名 .z01 即视为分卷 */
    size_t blen = strlen(zip_path);
    char z01[1200];
    if (blen > 4 && strcmp(zip_path + blen - 4, ".zip") == 0) {
        memcpy(z01, zip_path, blen - 4);
        strcpy(z01 + blen - 4, ".z01");
    } else {
        z01[0] = '\0';
    }
    if (z01[0] && fopen(z01, "rb")) {
        /* 分卷：逐个读 .z01 .z02 ... 直到 .zip */
        unsigned char *buf = NULL; size_t total = 0;
        int vol = 1;
        while (1) {
            char vp[1200];
            if (vol == 1) {
                strcpy(vp, z01);
            } else if (vol == 100) {
                /* .z100 不再是 3 位，良好的 zip 工具用 .zNN 到 99 后换 .zip? 规范只到 .z99 */
                break;
            } else {
                snprintf(vp, sizeof(vp), "%.*s.z%02d", (int)(blen - 4), zip_path, vol);
            }
            FILE *f = fopen(vp, "rb");
            if (!f) break;
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            unsigned char *nb = realloc(buf, total + (size_t)sz + 1);
            if (!nb) { fclose(f); free(buf); return RP_ERR_NOMEM; }
            buf = nb;
            if (fread(buf + total, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return RP_ERR_IO; }
            total += (size_t)sz;
            fclose(f);
            if (strcmp(vp, zip_path) == 0) break;  /* 末卷就是 .zip 本身 */
            vol++;
            if (vol > 999) break;
        }
        if (total == 0) { free(buf); return RP_ERR_IO; }
        *out = buf; *outlen = total;
        return RP_OK;
    }
    return read_all(zip_path, out, outlen);
}

static int make_dirs(const char *full)
{
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", full);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p; *p = '\0';
            if (tmp[0]) mkdir(tmp, 0755);
            *p = c;
        }
    }
    if (tmp[0]) mkdir(tmp, 0755);
    return RP_OK;
}

/* 仅创建 full 的父目录（含所有祖先级，用于写文件前确保目录存在） */
static int make_parent(const char *full)
{
    char tmp[3072];
    snprintf(tmp, sizeof(tmp), "%s", full);
    char *slash = NULL;
    for (char *p = tmp; *p; p++) if (*p == '/' || *p == '\\') slash = p;
    if (slash) { *slash = '\0'; if (tmp[0]) make_dirs(tmp); }
    return RP_OK;
}

/* 在 buf 尾部回扫 EOCD 签名（允许尾部有注释/自解压前缀） */
static unsigned char *find_eocd(unsigned char *buf, size_t len)
{
    size_t start = len > 22 ? len - 22 : 0;
    for (size_t i = start + 1; i + 22 <= len; i++) {
        if (buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 5 && buf[i+3] == 6)
            return buf + i;
    }
    /* 再整体回扫（注释可能较长） */
    size_t lim = len > 65557 ? len - 65557 : 0;
    for (size_t i = len >= 22 ? len - 22 : 0; i + 22 <= len && i >= lim; ) {
        if (buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 5 && buf[i+3] == 6)
            return buf + i;
        if (i == 0) break;
        i--;
    }
    return NULL;
}

int zip_extract(const char *zip_path, const char *out_dir, int *out_count)
{
    unsigned char *stream = NULL; size_t slen = 0;
    int rc = load_zip_stream(zip_path, &stream, &slen);
    if (rc != RP_OK) return rc;

    unsigned char *eocd = find_eocd(stream, slen);
    if (!eocd) { free(stream); return RP_ERR_FORMAT; }

    uint16_t n_entries = (uint16_t)(eocd[10] | (eocd[11] << 8));
    uint32_t cd_offset  = (uint32_t)(eocd[16] | (eocd[17] << 8) | (eocd[18] << 16) | ((uint32_t)eocd[19] << 24));

    int count = 0;
    unsigned char *p = stream + cd_offset;
    for (uint16_t i = 0; i < n_entries; i++) {
        if (p + 46 > stream + slen || p[0] != 'P' || p[1] != 'K' || p[2] != 1 || p[3] != 2) {
            rc = RP_ERR_FORMAT; goto done;
        }
        uint16_t method    = (uint16_t)(p[10] | (p[11] << 8));
        uint32_t crc       = (uint32_t)(p[16] | (p[17] << 8) | (p[18] << 16) | ((uint32_t)p[19] << 24));
        uint32_t comp_size = (uint32_t)(p[20] | (p[21] << 8) | (p[22] << 16) | ((uint32_t)p[23] << 24));
        uint32_t uncomp    = (uint32_t)(p[24] | (p[25] << 8) | (p[26] << 16) | ((uint32_t)p[27] << 24));
        uint16_t nlen      = (uint16_t)(p[28] | (p[29] << 8));
        uint16_t elen      = (uint16_t)(p[30] | (p[31] << 8));
        uint16_t clen      = (uint16_t)(p[32] | (p[33] << 8));
        char name[2048];
        if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
        memcpy(name, p + 46, nlen);
        name[nlen] = '\0';
        uint32_t lho = (uint32_t)(p[42] | (p[43] << 8) | (p[44] << 16) | ((uint32_t)p[45] << 24));
        p += 46 + nlen + elen + clen;

        /* 目录项（以 / 结尾）只建目录 */
        if (name[strlen(name) - 1] == '/') {
            char full[3072];
            snprintf(full, sizeof(full), "%s/%s", out_dir, name);
            make_dirs(full);
            count++;
            continue;
        }

        /* 读 local header 拿数据偏移 */
        if (stream + lho + 30 > stream + slen) { rc = RP_ERR_FORMAT; goto done; }
        unsigned char *lh = stream + lho;
        if (!(lh[0] == 'P' && lh[1] == 'K' && lh[2] == 3 && lh[3] == 4)) { rc = RP_ERR_FORMAT; goto done; }
        uint16_t lnl = (uint16_t)(lh[26] | (lh[27] << 8));
        uint16_t lel = (uint16_t)(lh[28] | (lh[29] << 8));
        unsigned char *data = lh + 30 + lnl + lel;
        if (data + comp_size > stream + slen) { rc = RP_ERR_FORMAT; goto done; }

        /* 解出 */
        unsigned char *raw = NULL; size_t rawlen = uncomp;
        if (method == 0) {
            raw = malloc(comp_size ? comp_size : 1);
            if (!raw) { rc = RP_ERR_NOMEM; goto done; }
            memcpy(raw, data, comp_size);
            rawlen = comp_size;
        } else if (method == 8) {
            raw = malloc(uncomp ? uncomp : 1);
            if (!raw) { rc = RP_ERR_NOMEM; goto done; }
            z_stream strm; memset(&strm, 0, sizeof(strm));
            if (inflateInit2(&strm, -15) != Z_OK) { free(raw); rc = RP_ERR_GENERIC; goto done; }
            strm.next_in = data; strm.avail_in = (uInt)comp_size;
            strm.next_out = raw; strm.avail_out = (uInt)uncomp;
            int zr = inflate(&strm, Z_FINISH);
            if (zr != Z_STREAM_END) { inflateEnd(&strm); free(raw); rc = RP_ERR_FORMAT; goto done; }
            rawlen = uncomp - strm.avail_out;
            inflateEnd(&strm);
        } else {
            rc = RP_ERR_NOTSUP; goto done;
        }

        if (crc32(0, raw, rawlen) != crc) {
            free(raw); rc = RP_ERR_FORMAT; goto done;
        }

        char full[3072];
        snprintf(full, sizeof(full), "%s/%s", out_dir, name);
        for (char *q = full; *q; q++) if (*q == '\\') *q = '/';
        make_parent(full);
        FILE *f = fopen(full, "wb");
        if (!f) { free(raw); rc = RP_ERR_IO; goto done; }
        fwrite(raw, 1, rawlen, f);
        fclose(f);
        free(raw);
        count++;
    }

    if (out_count) *out_count = count;
    rc = RP_OK;
done:
    free(stream);
    return rc;
}

int is_zip_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char sig[4];
    size_t rd = fread(sig, 1, 4, f);
    fclose(f);
    if (rd == 4 && sig[0] == 'P' && sig[1] == 'K' && (sig[2] == 3 && sig[3] == 4)) return 1;
    /* 分卷首卷 .z01 不是 PK 头，但可按扩展名/后缀判断 */
    size_t n = strlen(path);
    if (n > 4 && strcmp(path + n - 4, ".zip") == 0) return 1;
    if (n > 4 && strcmp(path + n - 4, ".z01") == 0) return 1;
    return 0;
}
