#define _POSIX_C_SOURCE 200809L
/* rpa/rpa_v3.c —— RPA-3.0 解包实现（与 Ren'Py 真实格式兼容）
 *
 * 布局： [24字节头][文件数据…][zlib 压缩的 pickle 索引]
 * 头： "RPA-3.0" + prefix(8hex) + index_offset(8hex) + "\n"
 * 索引每项为 (name -> [(offset^ (prefix<<32), length^prefix, prefix), …])
 */
#include "rpa_v3.h"
#include "compress/zlib_wrap.h"
#include "util/io_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <windows.h>
#endif

int rpa_v3_can_handle(const uint8_t *hdr, size_t len) {
    if (len < RPA3_HEADER_LEN) return 0;
    if (memcmp(hdr, RPA3_MAGIC, RPA3_MAGIC_LEN) != 0) return 0;
    for (int i = RPA3_MAGIC_LEN; i < RPA3_MAGIC_LEN + 16; i++) {
        char c = (char)hdr[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

int rpa_v3_read_header(const uint8_t *buf, size_t len, rpa_header_t *out) {
    if (len < RPA3_HEADER_LEN) return RP_ERR_FORMAT;
    if (memcmp(buf, RPA3_MAGIC, RPA3_MAGIC_LEN) != 0) return RP_ERR_FORMAT;
    char tmp[9];
    memcpy(tmp, buf + RPA3_MAGIC_LEN, 8); tmp[8] = 0;
    out->prefix = (uint32_t)strtoul(tmp, NULL, 16);
    memcpy(tmp, buf + RPA3_MAGIC_LEN + 8, 8); tmp[8] = 0;
    out->index_len = (uint32_t)strtoul(tmp, NULL, 16); /* 此处实为 index_offset */
    return RP_OK;
}

static void ensure_parent(const char *path) {
#ifndef _WIN32
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *cut = strrchr(tmp, '/');
    if (!cut) return;                 /* 无目录分量 */
    *cut = '\0';                      /* 截断到父目录（去掉文件名） */
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; if(mkdir(tmp,0755)!=0 && errno!=EEXIST) return; *p = '/'; }
    }
    if(mkdir(tmp,0755)!=0 && errno!=EEXIST) return;
#else
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *cut = strrchr(tmp, '/');
    char *bslash = strrchr(tmp, '\\');
    char *c = (cut > bslash) ? cut : bslash;
    if (!c) return;
    *c = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') { *p = 0; if(_mkdir(tmp)!=0 && errno!=EEXIST) return; *p = (c == cut) ? '/' : '\\'; }
    }
    if(_mkdir(tmp)!=0 && errno!=EEXIST) return;
#endif
}

static int write_file(const char *out_dir, const char *name, const uint8_t *data, size_t len) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", out_dir, name);
    ensure_parent(path);              /* 只创建父目录，不把文件名当目录 */
    FILE *f = fopen(path, "wb");
    if (!f) return RP_ERR_IO;
    fwrite(data, 1, len, f);
    fclose(f);
    return RP_OK;
}

void rpa_index_free(rpa_index_t *idx) {
    if (!idx) return;
    for (size_t i = 0; i < idx->nfiles; i++) {
        free(idx->files[i].name);
        free(idx->files[i].blocks);
    }
    free(idx->files);
    idx->files = NULL; idx->nfiles = 0;
}

int rpa_index_from_pickle(const pv_value *root, rpa_index_t *out) {
    if (!root || root->type != PV_DICT) return RP_ERR_FORMAT;
    memset(out, 0, sizeof(*out));
    out->nfiles = root->nk;
    out->files = (rpa_file_t*)calloc(root->nk ? root->nk : 1, sizeof(rpa_file_t));
    if (!out->files) return RP_ERR_NOMEM;
    for (size_t i = 0; i < root->nk; i++) {
        const pv_value *k = root->keys[i], *v = root->vals[i];
        if (!k || k->type != PV_STR || !v || v->type != PV_LIST) { rpa_index_free(out); return RP_ERR_FORMAT; }
        out->files[i].name = strdup(k->s);
        out->files[i].nblocks = v->n;
        out->files[i].blocks = (rpa_block_t*)calloc(v->n ? v->n : 1, sizeof(rpa_block_t));
        for (size_t j = 0; j < v->n; j++) {
            const pv_value *t = v->items[j];
            if (!t || t->type != PV_TUPLE || t->n != 3) { rpa_index_free(out); return RP_ERR_FORMAT; }
            out->files[i].blocks[j].offset = t->items[0]->u;
            out->files[i].blocks[j].length = t->items[1]->u;
            out->files[i].blocks[j].prefix = (uint32_t)t->items[2]->u;
        }
    }
    return RP_OK;
}

int rpa_v3_unpack(const char *in_file, const char *out_dir) {
    uint8_t *whole = NULL; size_t fsz = 0;
    int rc = read_file_all(in_file, &whole, &fsz);
    if (rc != RP_OK) return rc;

    rpa_header_t hdr;
    rc = rpa_v3_read_header(whole, fsz, &hdr);
    if (rc != RP_OK) goto done;

    uint64_t index_off = (uint64_t)hdr.index_len;   /* 字段含义：索引起始偏移 */
    if (index_off + 8 > fsz) { rc = RP_ERR_FORMAT; goto done; }

    /* 索引从 index_off 读到文件末尾（与 Ren'Py 一致） */
    uint8_t *idx = NULL; size_t idxlen = fsz - index_off;
    rc = zlib_inflate(whole + index_off, idxlen, &idx, &idxlen);
    if (rc != RP_OK) goto done;

    pv_value *root = NULL;
    rc = pv_loads(idx, idxlen, &root);
    if (rc != RP_OK) { free(idx); goto done; }

    rpa_index_t ix; memset(&ix, 0, sizeof(ix));
    rc = rpa_index_from_pickle(root, &ix);
    if (rc != RP_OK) { pv_free(root); free(idx); goto done; }

    for (size_t i = 0; i < ix.nfiles; i++) {
        rpa_file_t *f = &ix.files[i];
        for (size_t b = 0; b < f->nblocks; b++) {
            uint64_t ro = f->blocks[b].offset ^ ((uint64_t)f->blocks[b].prefix << 32);
            uint64_t rl = f->blocks[b].length ^ f->blocks[b].prefix;
            if (ro + rl > fsz) { rc = RP_ERR_FORMAT; break; }
            rc = write_file(out_dir, f->name, whole + ro, (size_t)rl);
            if (rc != RP_OK) break;
        }
        if (rc != RP_OK) break;
    }
    rpa_index_free(&ix);
    pv_free(root);
    free(idx);

done:
    free(whole);
    return rc;
}

int rpa_v3_stat(const char *path, size_t *nfiles) {
    if (nfiles) *nfiles = 0;
    uint8_t *whole = NULL; size_t fsz = 0;
    int rc = read_file_all(path, &whole, &fsz);
    if (rc != RP_OK) return rc;

    rpa_header_t hdr;
    if ((rc = rpa_v3_read_header(whole, fsz, &hdr)) != RP_OK) { free(whole); return rc; }
    uint64_t index_off = (uint64_t)hdr.index_len;
    if (index_off + 8 > fsz) { free(whole); return RP_ERR_FORMAT; }

    uint8_t *idx = NULL; size_t idxlen = fsz - index_off;
    if ((rc = zlib_inflate(whole + index_off, idxlen, &idx, &idxlen)) != RP_OK) { free(whole); return rc; }

    pv_value *root = NULL;
    if ((rc = pv_loads(idx, idxlen, &root)) != RP_OK) { free(idx); free(whole); return rc; }
    if (root && root->type == PV_DICT && nfiles) *nfiles = root->nk;
    pv_free(root); free(idx); free(whole);
    return RP_OK;
}
