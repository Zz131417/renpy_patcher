/* rpa/rpa_v3_repack.c —— RPA-3.0 重打包（与 Ren'Py 真实格式兼容）
 *
 * 布局： [24字节头][文件数据…][zlib 压缩的 pickle 索引]
 * 头部第三字段 = 索引在文件中的真实偏移（index_offset）。
 */
#include "rpa_v3.h"
#include "compress/zlib_wrap.h"
#include "util/pickle_write.h"
#include "util/dir_walk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define RPA3_PREFIX 0x12345678u

int rpa_v3_repack(const char *dir, const char *out_file) {
    file_entry_t *files = NULL;
    size_t nfiles = 0;
    int rc = collect_files(dir, &files, &nfiles);
    if (rc != RP_OK) return rc;

    rpa_block_out_t *blocks = (rpa_block_out_t *)calloc(nfiles ? nfiles : 1, sizeof(rpa_block_out_t));
    rpa_file_out_t  *flist  = (rpa_file_out_t  *)calloc(nfiles ? nfiles : 1, sizeof(rpa_file_out_t));
    if (!blocks || !flist) { rc = RP_ERR_NOMEM; goto cleanup; }

    uint32_t prefix = RPA3_PREFIX;
    uint64_t prefix_hi = ((uint64_t)prefix << 32);

    /* 第一遍：写 24 字节占位头 + 文件数据，记录每块真实偏移/长度 */
    FILE *o = fopen(out_file, "wb");
    if (!o) { rc = RP_ERR_IO; goto cleanup; }

    char header[32];
    int hl = snprintf(header, sizeof(header), "%s%08x%08x\n",
                      RPA3_MAGIC, prefix, (unsigned)0);   /* 偏移稍后回填 */
    fwrite(header, 1, (size_t)hl, o);

    uint64_t off = RPA3_HEADER_LEN;
    for (size_t i = 0; i < nfiles; i++) {
        FILE *f = fopen(files[i].full_path, "rb");
        if (!f) { fprintf(stderr, "警告：无法读取 %s，跳过\n", files[i].full_path); continue; }
        uint8_t buf[65536]; size_t r;
        while ((r = fread(buf, 1, sizeof(buf), f)) > 0) fwrite(buf, 1, r, o);
        fclose(f);

        blocks[i].offset = off ^ prefix_hi;
        blocks[i].length = files[i].size ^ prefix;
        blocks[i].prefix = prefix;
        flist[i].name    = files[i].rel_path;
        flist[i].nblocks = 1;
        flist[i].blocks  = &blocks[i];
        off += files[i].size;
    }
    uint64_t index_off = off;   /* 索引起始偏移 = 24 + 总数据长度 */

    /* 序列化索引并压缩，写在数据之后 */
    uint8_t *pk = NULL; size_t pklen = 0;
    rc = pickle_write_rpa_index(flist, nfiles, &pk, &pklen);
    if (rc != RP_OK) { fclose(o); goto cleanup; }
    uint8_t *comp = NULL; size_t complen = 0;
    rc = zlib_deflate(pk, pklen, &comp, &complen, -1);
    free(pk);
    if (rc != RP_OK) { fclose(o); goto cleanup; }
    fwrite(comp, 1, complen, o);
    fclose(o);

    /* 回填头部 index_offset 字段（第 15..22 字节） */
    FILE *h = fopen(out_file, "r+b");
    if (h) {
        fseek(h, 0, SEEK_SET);
        char realhdr[32];
        int rl = snprintf(realhdr, sizeof(realhdr), "%s%08x%08x\n",
                          RPA3_MAGIC, prefix, (unsigned)index_off);
        fwrite(realhdr, 1, (size_t)rl, h);
        fclose(h);
    }

    printf("packed: %zu file(s) -> %s (index @%llu, %zu bytes)\n",
           nfiles, out_file, (unsigned long long)index_off, complen);

cleanup:
    free(comp);
    free(blocks);
    free(flist);
    free_files(files, nfiles);
    return rc;
}
