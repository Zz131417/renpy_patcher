/* compress/zlib_wrap.c —— 见 zlib_wrap.h */
#include "zlib_wrap.h"
#include <stdio.h>
#include "core/plugin.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int zlib_inflate(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen) {
    z_stream z;
    memset(&z, 0, sizeof(z));
    if (inflateInit(&z) != Z_OK) return RP_ERR_GENERIC;

    z.next_in = (Bytef*)in;
    z.avail_in = (uInt)inlen;

    size_t cap = inlen * 4 + 4096;
    uint8_t *buf = (uint8_t*)malloc(cap);
    if (!buf) { inflateEnd(&z); return RP_ERR_NOMEM; }

    int rc = RP_OK;
    while (1) {
        z.next_out = buf + z.total_out;
        z.avail_out = (uInt)(cap - z.total_out);
        int r = inflate(&z, Z_FINISH);
        if (r == Z_STREAM_END) break;
        if (r == Z_OK) {
            cap *= 2;
            uint8_t *nb = (uint8_t*)realloc(buf, cap);
            if (!nb) { rc = RP_ERR_NOMEM; goto done; }
            buf = nb;
            continue;
        }
        rc = RP_ERR_FORMAT;
        goto done;
    }
    *out = buf;
    *outlen = z.total_out;
    buf = NULL;
done:
    inflateEnd(&z);
    free(buf);
    return rc;
}

int zlib_deflate(const uint8_t *in, size_t inlen, uint8_t **out, size_t *outlen, int level) {
    z_stream z;
    memset(&z, 0, sizeof(z));
    if (level < 0) level = Z_DEFAULT_COMPRESSION;
    if (deflateInit(&z, level) != Z_OK) return RP_ERR_GENERIC;

    z.next_in = (Bytef*)in;
    z.avail_in = (uInt)inlen;

    size_t cap = inlen + inlen / 4 + 1024;
    uint8_t *buf = (uint8_t*)malloc(cap);
    if (!buf) { deflateEnd(&z); return RP_ERR_NOMEM; }

    int rc = RP_OK;
    int flush = Z_FINISH;
    while (1) {
        z.next_out = buf + z.total_out;
        z.avail_out = (uInt)(cap - z.total_out);
        int r = deflate(&z, flush);
        if (r == Z_STREAM_END) break;
        if (r == Z_OK) {
            cap *= 2;
            uint8_t *nb = (uint8_t*)realloc(buf, cap);
            if (!nb) { rc = RP_ERR_NOMEM; goto done; }
            buf = nb;
            continue;
        }
        rc = RP_ERR_GENERIC;
        goto done;
    }
    *out = buf;
    *outlen = z.total_out;
    buf = NULL;
done:
    deflateEnd(&z);
    free(buf);
    return rc;
}
