#define _POSIX_C_SOURCE 200809L
/* translate/json_io.c —— 可翻译文本条目 JSON 读写（最小自研，无第三方依赖） */
#include "translate/json_io.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- 写 ---------- */

static void json_esc(FILE *f, const char *s) {
    fputc('"', f);
    for (const char *p = s; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            case '\b': fputs("\\b", f); break;
            case '\f': fputs("\\f", f); break;
            default:
                if (c < 0x20) {
                    fprintf(f, "\\u%04x", c);
                } else {
                    fputc((int)c, f);
                }
        }
    }
    fputc('"', f);
}

int write_dialogs_json(const dialog_entry_t *arr, size_t count, const char *path) {
    FILE *f = fopen(path, "wb");   /* UTF-8 原文写入 */
    if (!f) return RP_ERR_IO;
    fputs("[\n", f);
    for (size_t i = 0; i < count; i++) {
        fprintf(f, "  {\"file\":");
        json_esc(f, arr[i].file ? arr[i].file : "");
        fprintf(f, ",\"line\":%d,\"type\":", arr[i].line);
        json_esc(f, arr[i].type ? arr[i].type : "say");
        fputs(",\"speaker\":", f);
        json_esc(f, arr[i].speaker ? arr[i].speaker : "");
        fputs(",\"source\":", f);
        json_esc(f, arr[i].source ? arr[i].source : "");
        fputs(",\"target\":", f);
        json_esc(f, arr[i].target ? arr[i].target : "");
        fputs(i + 1 < count ? "},\n" : "}\n", f);
    }
    fputs("]\n", f);
    fclose(f);
    log_info("导出 JSON: %s (%zu 条)", path, count);
    return RP_OK;
}

/* ---------- 读 ---------- */

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* 解析一个 JSON 字符串（*pp 指向开头引号），返回内容（需 free） */
static const char *parse_str(const char *p, char **out) {
    if (*p != '"') return NULL;
    p++;
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    while (*p) {
        if (*p == '\\' && p[1]) {
            char n = p[1]; p += 2;
            switch (n) {
                case '"': buf[len++]='"'; break;
                case '\\': buf[len++]='\\'; break;
                case '/': buf[len++]='/'; break;
                case 'b': buf[len++]='\b'; break;
                case 'f': buf[len++]='\f'; break;
                case 'n': buf[len++]='\n'; break;
                case 'r': buf[len++]='\r'; break;
                case 't': buf[len++]='\t'; break;
                case 'u': {
                    int v = 0;
                    for (int k = 0; k < 4 && p[0] && p[1] && p[2] && p[3]; k++) {
                        char h = p[k];
                        v <<= 4;
                        if (h >= '0' && h <= '9') v |= h - '0';
                        else if (h >= 'a' && h <= 'f') v |= h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') v |= h - 'A' + 10;
                    }
                    p += 4;
                    if (v < 0x80) { if(len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=(char)v; }
                    else if (v < 0x800) {
                        if(len+2>=cap){cap*=2;buf=realloc(buf,cap);}
                        buf[len++]=(char)(0xC0|(v>>6)); buf[len++]=(char)(0x80|(v&0x3F));
                    } else {
                        if(len+3>=cap){cap*=2;buf=realloc(buf,cap);}
                        buf[len++]=(char)(0xE0|(v>>12)); buf[len++]=(char)(0x80|((v>>6)&0x3F)); buf[len++]=(char)(0x80|(v&0x3F));
                    }
                } break;
                default: buf[len++] = n; break;
            }
            continue;
        }
        if (*p == '"') { p++; buf[len]=0; *out=buf; return p; }
        if (len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=*p++;
    }
    free(buf);
    return NULL;
}

/* 解析一个整数 */
static const char *parse_int(const char *p, int *out) {
    char *end; long v = strtol(p, &end, 10);
    if (end == p) return NULL;
    *out = (int)v;
    return end;
}

/* 读一个对象，填充 e；返回对象之后的指针 */
static const char *parse_obj(const char *p, dialog_entry_t *e) {
    memset(e, 0, sizeof(*e));
    e->speaker = strdup("");
    e->target = strdup("");
    e->type = strdup("say");
    if (*p != '{') return NULL;
    p++;
    while (*p) {
        p = skip_ws(p);
        if (*p == '}') return p + 1;
        if (*p != '"') return NULL;
        char *key; p = parse_str(p, &key);
        if (!p) return NULL;
        p = skip_ws(p);
        if (*p != ':') { free(key); return NULL; }
        p = skip_ws(p + 1);
        if (*p == '"') {
            char *val; p = parse_str(p, &val);
            if (!p) { free(key); return NULL; }
            if      (!strcmp(key, "file"))    e->file = val;
            else if (!strcmp(key, "speaker")) { free(e->speaker); e->speaker = val; }
            else if (!strcmp(key, "source"))  e->source = val;
            else if (!strcmp(key, "target"))  { free(e->target); e->target = val; }
            else if (!strcmp(key, "type"))    { free(e->type); e->type = val; }
            else free(val);
        } else if (isdigit((unsigned char)*p) || *p=='-') {
            int v; const char *np = parse_int(p, &v);
            if (!np) { free(key); return NULL; }
            if (!strcmp(key, "line")) e->line = v;
            p = np;
        } else { free(key); return NULL; }
        free(key);
        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') return p + 1;
        return NULL;
    }
    return NULL;
}

int read_dialogs_json(const char *path, dialog_entry_t **out, size_t *count) {
    FILE *f = fopen(path, "rb");
    if (!f) return RP_ERR_IO;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return RP_ERR_NOMEM; }
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = 0; fclose(f);

    const char *p = skip_ws(buf);
    if (*p != '[') { free(buf); return RP_ERR_FORMAT; }
    p++;
    size_t cap = 32, n = 0;
    dialog_entry_t *arr = malloc(cap * sizeof(*arr));
    while (*p) {
        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == '{') {
            dialog_entry_t e;
            const char *np = parse_obj(p, &e);
            if (!np) { free(buf); free_dialog_entries(arr, n); return RP_ERR_FORMAT; }
            if (n >= cap) { cap *= 2; arr = realloc(arr, cap * sizeof(*arr)); }
            arr[n++] = e;
            p = np;
            p = skip_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == ']') break;
            free(buf); free_dialog_entries(arr, n); return RP_ERR_FORMAT;
        }
        break;
    }
    free(buf);
    *out = arr; *count = n;
    log_info("读取 JSON: %s (%zu 条)", path, n);
    return RP_OK;
}
