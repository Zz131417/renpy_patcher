/* util/io_buf.c —— 见 io_buf.h */
#include "io_buf.h"
#include "core/plugin.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int read_file_all(const char *path, uint8_t **out, size_t *outlen) {
    FILE *f = fopen(path, "rb");
    if (!f) return RP_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return RP_ERR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return RP_ERR_IO; }
    rewind(f);
    uint8_t *buf = (uint8_t*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return RP_ERR_NOMEM; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;
    *out = buf;
    *outlen = rd;
    return RP_OK;
}

void bb_init(byte_buf *b) { b->data = NULL; b->len = 0; b->cap = 0; }

int bb_append(byte_buf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->len + n) nc *= 2;
        uint8_t *nb = (uint8_t*)realloc(b->data, nc);
        if (!nb) return RP_ERR_NOMEM;
        b->data = nb; b->cap = nc;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
    return RP_OK;
}

void bb_free(byte_buf *b) { free(b->data); b->data = NULL; b->len = b->cap = 0; }
