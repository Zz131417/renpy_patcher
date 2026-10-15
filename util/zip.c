/* util/zip.c —— 见 util/zip.h
 *
 * 极简 ZIP 写入器（PKWARE APPNOTE 规范子集）：
 *   - 支持 store(0) 与 deflate(8)（deflate 经 zlib 的裸 deflate 流）
 *   - 用 zlib 的 crc32 计算校验
 *   - 产出标准 .zip，可被系统 unzip / Windows 资源管理器 / Python zipfile 读取
 *   - 可选分卷：按固定字节数切分为 .z01 ... .zip；按 cat 重组即还原原字节流
 *
 * 不引入任何第三方库（仅 zlib），保持与项目一致的零运行时依赖。
 */
#include "util/zip.h"
#include "core/plugin.h"
#include "core/plugin_loader.h"
#include "util/log.h"
#include "util/dir_walk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

/* ---------- 动态缓冲 ---------- */
typedef struct { unsigned char *p; size_t len, cap; } dyn_t;

static int dyn_append(dyn_t *d, const void *buf, size_t n) {
    if (d->len + n > d->cap) {
        size_t nc = d->cap ? d->cap * 2 : 4096;
        while (nc < d->len + n) nc *= 2;
        unsigned char *np = realloc(d->p, nc);
        if (!np) return -1;
        d->p = np; d->cap = nc;
    }
    memcpy(d->p + d->len, buf, n);
    d->len += n;
    return 0;
}

/* ---------- ZIP 结构 ---------- */
struct zip_writer {
    char   base[1024];      /* 去掉 .zip 后缀后的基名 */
    char   out_path[1100];  /* 单文件时的精确输出路径 */
    size_t split_size;      /* 0=单文件 */
    uint16_t align;         /* 条目数据对齐（0/1=不填充，APK 用 4） */
    dyn_t  body;            /* 所有 local header + 数据 */
    dyn_t  cd;              /* 中央目录 */
    uint16_t n_entries;
};

static void dos_datetime(uint16_t *t, uint16_t *d) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm) { *t = 0; *d = 0x21; return; }   /* 1980-01-01 兜底 */
    *d = (uint16_t)(((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday);
    *t = (uint16_t)((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
}

static int read_file(const char *path, unsigned char **out, size_t *outlen) {
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

/* 裸 deflate（无 zlib 头），返回堆缓冲 */
static int deflate_raw(const unsigned char *in, size_t inlen,
                       unsigned char **out, size_t *outlen) {
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        return RP_ERR_GENERIC;
    size_t cap = inlen + inlen / 16 + 64;
    if (cap < 64) cap = 64;
    unsigned char *buf = malloc(cap);
    if (!buf) { deflateEnd(&strm); return RP_ERR_NOMEM; }
    strm.next_in = (Bytef *)in;
    strm.avail_in = (uInt)inlen;
    strm.next_out = buf;
    strm.avail_out = (uInt)cap;
    int rc = deflate(&strm, Z_FINISH);
    while (rc == Z_OK) {                 /* 缓冲不够则扩容续压 */
        size_t used = cap - strm.avail_out;
        cap *= 2;
        unsigned char *nb = realloc(buf, cap);
        if (!nb) { free(buf); deflateEnd(&strm); return RP_ERR_NOMEM; }
        buf = nb;
        strm.next_out = buf + used;
        strm.avail_out = (uInt)(cap - used);
        rc = deflate(&strm, Z_FINISH);
    }
    if (rc != Z_STREAM_END) { free(buf); deflateEnd(&strm); return RP_ERR_GENERIC; }
    *outlen = cap - strm.avail_out;
    *out = buf;
    deflateEnd(&strm);
    return RP_OK;
}

zip_writer_t *zip_open(const char *out_path, size_t split_size) {
    if (!out_path || !*out_path) return NULL;
    zip_writer_t *z = calloc(1, sizeof(*z));
    if (!z) return NULL;
    const char *dot = strrchr(out_path, '.');
    if (dot && strcmp(dot, ".zip") == 0) {
        size_t n = (size_t)(dot - out_path);
        if (n >= sizeof(z->base)) n = sizeof(z->base) - 1;
        memcpy(z->base, out_path, n);
        z->base[n] = '\0';
    } else {
        snprintf(z->base, sizeof(z->base), "%s", out_path);
    }
    snprintf(z->out_path, sizeof(z->out_path), "%s", out_path);
    z->split_size = split_size;
    z->align = 0;
    return z;
}

static int zip_add_internal(zip_writer_t *z, const char *arc_name,
                            const unsigned char *data, size_t len, int compress) {
    uint32_t crc = (uint32_t)crc32(0, data, len);
    unsigned char *cdata = (unsigned char *)data;
    size_t clen = len;
    int need_free = 0;
    if (compress) {
        if (deflate_raw(data, len, &cdata, &clen) != RP_OK) return RP_ERR_GENERIC;
        need_free = 1;
    }

    uint16_t mtime, mdate;
    dos_datetime(&mtime, &mdate);
    uint16_t method = compress ? 8 : 0;

    size_t lho = z->body.len;   /* 本文件 local header 在 body 中的偏移 */
    uint16_t nl = (uint16_t)strlen(arc_name);

    /* 对齐：通过对齐 extra 字段（local header 末尾的 extra 区）让【数据起始偏移】
     * 落在 align 字节边界（APK 要求 4 字节对齐 / zipalign）。
     * 填充字节写在 extra 区，不进入压缩流，读取方按 extra-length 跳过即可。 */
    uint16_t pad = 0;
    if (z->align > 1) {
        uint16_t a = z->align;
        size_t off = lho + 30 + nl;        /* 数据本应起始处（无 extra 时） */
        pad = (uint16_t)((a - (off % a)) % a);
    }

    /* local file header (30 字节 + name) */
    unsigned char lh[30];
    memset(lh, 0, sizeof(lh));
    lh[0] = 'P'; lh[1] = 'K'; lh[2] = 3; lh[3] = 4;
    lh[4] = 20;                                   /* version needed */
    /* flags=0, method */
    lh[8] = (unsigned char)method; lh[9] = 0;
    lh[10] = (unsigned char)(mtime & 0xff); lh[11] = (unsigned char)(mtime >> 8);
    lh[12] = (unsigned char)(mdate & 0xff); lh[13] = (unsigned char)(mdate >> 8);
    memcpy(lh + 14, &crc, 4);
    memcpy(lh + 18, &clen, 4);
    memcpy(lh + 22, &len, 4);
    memcpy(lh + 26, &nl, 2);
    lh[28] = (unsigned char)(pad & 0xff);          /* extra length = 对齐填充 */
    lh[29] = (unsigned char)(pad >> 8);
    dyn_append(&z->body, lh, 30);
    dyn_append(&z->body, arc_name, nl);
    if (pad) {                                    /* extra 区填充（对齐） */
        static const unsigned char zeros[16] = {0};
        size_t off = 0;
        while (off < pad) {
            size_t chunk = pad - off; if (chunk > 16) chunk = 16;
            dyn_append(&z->body, zeros, chunk);
            off += chunk;
        }
    }
    dyn_append(&z->body, cdata, clen);

    /* central directory header (46 字节 + name) */
    unsigned char ch[46];
    memset(ch, 0, sizeof(ch));
    ch[0] = 'P'; ch[1] = 'K'; ch[2] = 1; ch[3] = 2;
    ch[4] = 20; ch[5] = 0;                          /* version made by */
    ch[6] = 20; ch[7] = 0;                          /* version needed */
    /* flags=0, method */
    ch[10] = (unsigned char)method; ch[11] = 0;
    ch[12] = (unsigned char)(mtime & 0xff); ch[13] = (unsigned char)(mtime >> 8);
    ch[14] = (unsigned char)(mdate & 0xff); ch[15] = (unsigned char)(mdate >> 8);
    memcpy(ch + 16, &crc, 4);
    memcpy(ch + 20, &clen, 4);
    memcpy(ch + 24, &len, 4);
    memcpy(ch + 28, &nl, 2);
    ch[30] = 0; ch[31] = 0;                         /* extra len（中央目录不写对齐填充，仅 local header 承载） */
    ch[32] = 0; ch[33] = 0;                         /* comment len */
    ch[34] = 0; ch[35] = 0;                         /* disk number start */
    ch[36] = 0; ch[37] = 0;                         /* internal attr */
    ch[38] = 0; ch[39] = 0; ch[40] = 0; ch[41] = 0; /* external attr */
    uint32_t lho32 = (uint32_t)lho;
    memcpy(ch + 42, &lho32, 4);
    dyn_append(&z->cd, ch, 46);
    dyn_append(&z->cd, arc_name, nl);

    z->n_entries++;
    if (need_free) free(cdata);
    return RP_OK;
}

int zip_add_file_data(zip_writer_t *z, const char *arc_name,
                      const void *data, size_t len, int compress) {
    if (!z || !arc_name) return RP_ERR_GENERIC;
    return zip_add_internal(z, arc_name, (const unsigned char *)data, len, compress);
}

int zip_add_file(zip_writer_t *z, const char *arc_name,
                 const char *disk_path, int compress) {
    unsigned char *data; size_t len;
    int rc = read_file(disk_path, &data, &len);
    if (rc != RP_OK) return rc;
    rc = zip_add_internal(z, arc_name, data, len, compress);
    free(data);
    return rc;
}

int zip_add_dir(zip_writer_t *z, const char *dir_path,
                const char *arc_root, int compress) {
    file_entry_t *files = NULL; size_t n = 0;
    int rc = collect_files(dir_path, &files, &n);
    if (rc != RP_OK) return rc;
    for (size_t i = 0; i < n; i++) {
        /* 包内路径 = arc_root + '/' + rel_path（如有） */
        char arc[2048];
        if (arc_root && *arc_root)
            snprintf(arc, sizeof(arc), "%s/%s", arc_root, files[i].rel_path);
        else
            snprintf(arc, sizeof(arc), "%s", files[i].rel_path);
        /* 统一用 '/' */
        for (char *p = arc; *p; p++) if (*p == '\\') *p = '/';
        unsigned char *data; size_t len;
        if (read_file(files[i].full_path, &data, &len) == RP_OK) {
            zip_add_internal(z, arc, data, len, compress);
            free(data);
        }
    }
    free_files(files, n);
    return RP_OK;
}

int zip_set_alignment(zip_writer_t *z, uint16_t align) {
    if (!z) return RP_ERR_GENERIC;
    z->align = (align < 2) ? 0 : align;   /* 0/1 表示不填充 */
    return RP_OK;
}

int zip_close(zip_writer_t *z) {
    if (!z) return RP_ERR_GENERIC;

    /* 中央目录偏移 = 当前 body 长度（紧接数据之后） */
    uint32_t cd_offset = (uint32_t)z->body.len;

    /* 把 cd 追加到 body（形成完整 zip 流） */
    dyn_append(&z->body, z->cd.p, z->cd.len);

    /* EOCD (22 字节) */
    unsigned char eo[22];
    memset(eo, 0, sizeof(eo));
    eo[0] = 'P'; eo[1] = 'K'; eo[2] = 5; eo[3] = 6;
    /* disk numbers = 0 */
    uint16_t ne = z->n_entries;
    memcpy(eo + 8, &ne, 2);
    memcpy(eo + 10, &ne, 2);
    uint32_t cdsize = (uint32_t)z->cd.len;
    memcpy(eo + 12, &cdsize, 4);
    memcpy(eo + 16, &cd_offset, 4);
    eo[20] = 0; eo[21] = 0;                         /* comment len */
    dyn_append(&z->body, eo, 22);

    /* 写出 */
    int rc = RP_OK;
    size_t total = z->body.len;
    if (z->split_size == 0 || total == 0) {
        char path[1100];
        snprintf(path, sizeof(path), "%s", z->out_path);   /* 精确输出路径（可非 .zip，如 .apk） */
        FILE *f = fopen(path, "wb");
        if (!f) { rc = RP_ERR_IO; goto done; }
        fwrite(z->body.p, 1, total, f);
        fclose(f);
    } else {
        int N = (int)((total + z->split_size - 1) / z->split_size);
        if (N < 1) N = 1;
        if (N == 1) {
            char path[1100];
            snprintf(path, sizeof(path), "%s.zip", z->base);
            FILE *f = fopen(path, "wb");
            if (!f) { rc = RP_ERR_IO; goto done; }
            fwrite(z->body.p, 1, total, f);
            fclose(f);
        } else {
            size_t off = 0;
            for (int vol = 1; vol <= N; vol++) {
                size_t chunk = (vol == N) ? (total - off) : z->split_size;
                char path[1100];
                if (vol == N) snprintf(path, sizeof(path), "%s.zip", z->base);
                else         snprintf(path, sizeof(path), "%s.z%02d", z->base, vol);
                FILE *f = fopen(path, "wb");
                if (!f) { rc = RP_ERR_IO; goto done; }
                fwrite(z->body.p + off, 1, chunk, f);
                fclose(f);
                off += chunk;
                log_info("写出分卷 %s (%zu 字节)", path, chunk);
            }
        }
    }

done:
    free(z->body.p); free(z->cd.p); free(z);
    return rc;
}
