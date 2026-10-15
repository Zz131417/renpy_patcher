/* translate/rpy_dialog.c —— Ren'Py 可翻译文本提取 / 翻译 mod 生成
 *
 * 提取范围：
 *   - 对话(say)：说话人+字符串 / 裸字符串 / 多行三引号
 *   - 菜单(menu)：menu 选项文本（"选项": / "选项" if ...:）
 *   - 界面(ui)：screen 内显示文本（text / textbutton / button 的 text / hover / idle / tooltip 等）
 *
 * 中间交换格式为 JSON（见 json_io.c）。生成翻译 mod 用 Ren'Py 的 old/new 字符串翻译，
 * 对三种类型统一生效（Ren'Py 会自动匹配对话、菜单选项与界面显示文本）。
 */
#include "translate/rpy_dialog.h"
#include "util/dir_walk.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

/* ---------- 小工具 ---------- */

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int is_ident_start(int c) {
    return isalpha((unsigned char)c) || c == '_' || c > 127;
}
static int is_ident_cont(int c) {
    return isalnum((unsigned char)c) || c == '_' || c > 127;
}

static void strip_comment(char *line) {
    int in_s = 0, in_d = 0;
    for (char *p = line; *p; p++) {
        if (*p == '\'' && !in_d) in_s = !in_s;
        else if (*p == '"' && !in_s) in_d = !in_d;
        else if (*p == '#' && !in_s && !in_d) { *p = 0; break; }
    }
}

/* 解析 Ren'Py 字符串（支持 ' " 以及三引号 """） */
static const char *parse_string(const char *p, char **out) {
    char q = *p;
    int triple = 0;
    if (q == '"' && p[1] == '"' && p[2] == '"') { triple = 1; p += 3; }
    else if (q == '\'' && p[1] == '\'' && p[2] == '\'') { triple = 1; p += 3; }
    else { p += 1; }

    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    while (*p) {
        if (!triple) {
            if (*p == '\\' && p[1]) {
                char n = p[1]; char add;
                switch (n) {
                    case 'n': add='\n'; break; case 't': add='\t'; break;
                    case 'r': add='\r'; break; case '\\': add='\\'; break;
                    case '\'': add='\''; break; case '"': add='"'; break;
                    case '\n': add='\n'; break;
                    default: add=n; break;
                }
                if (len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=add;
                p += 2; continue;
            }
            if (*p == q) { buf[len]=0; *out=buf; return p+1; }
            if (len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=*p++;
        } else {
            if (*p == '"' && p[1] == '"' && p[2] == '"') { buf[len]=0; *out=buf; return p+3; }
            if (*p == '\'' && p[1] == '\'' && p[2] == '\'') { buf[len]=0; *out=buf; return p+3; }
            if (*p == '\\' && p[1]) {
                char n=p[1]; char add=n;
                if(n=='n')add='\n'; else if(n=='t')add='\t'; else if(n=='r')add='\r';
                else if(n=='"')add='"'; else if(n=='\'')add='\''; else if(n=='\\')add='\\';
                if(len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=add; p+=2; continue;
            }
            if (len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=*p++;
        }
    }
    free(buf);
    return NULL;
}

static int mkpath(const char *path) {
    char tmp[2048];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) return -1;
    memcpy(tmp, path, n + 1);
    for (size_t i = 1; i <= n; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\' || i == n) {
            char sep = tmp[i];
            tmp[i] = 0;
#ifdef _WIN32
            if (tmp[0]) _mkdir(tmp);
#else
            if (tmp[0]) mkdir(tmp, 0755);
#endif
            tmp[i] = sep;
        }
    }
    return 0;
}

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* 界面显示文本关键字（其后第一个字符串作为可翻译界面文本） */
static const char *ui_kw[] = {
    "text ", "textbutton ", "button ", "hover ", "idle ",
    "selected_idle ", "selected_hover ", "tooltip ", NULL
};

/* 菜单选项：行首直接是引号字符串，其后紧跟 ':' 或 "if" */
static int parse_menu_option(const char *work, char **source, char **anchor) {
    const char *p = skip_ws(work);
    if (*p != '"' && *p != '\'') return 0;
    char *s; const char *after = parse_string(p, &s);
    if (!after) { free(s); return 0; }
    const char *a = skip_ws(after);
    if (*a == ':' || (a[0] == 'i' && a[1] == 'f' && (a[2] == ' ' || a[2] == '\t' || a[2] == '('))) {
        *source = s;
        size_t alen = (size_t)(a - work);
        char *anc = malloc(alen + 1); memcpy(anc, work, alen); anc[alen] = 0;
        *anchor = anc;
        return 1;
    }
    free(s);
    return 0;
}

/* 界面文本：行以 ui 关键字开头，其后第一个字符串作为可翻译文本 */
static int parse_ui_line(const char *work, char **source, char **anchor) {
    for (int i = 0; ui_kw[i]; i++) {
        size_t k = strlen(ui_kw[i]);
        if (strncmp(work, ui_kw[i], k) == 0) {
            const char *p = skip_ws(work + k);
            if (*p == '"' || *p == '\'') {
                char *s; const char *after = parse_string(p, &s);
                if (after) {
                    *source = s;
                    size_t alen = (size_t)(p - work);
                    char *anc = malloc(alen + 1); memcpy(anc, work, alen); anc[alen] = 0;
                    *anchor = anc;
                    return 1;
                }
            }
            return 0; /* 关键字匹配但无字面量字符串，不算 */
        }
    }
    return 0;
}

static int is_display_keyword(const char *id) {
    static const char *kw[] = {
        "text", "textbutton", "button", "label", "image", "add", "hbox", "vbox",
        "grid", "frame", "window", "viewport", "dismiss", "key", "on", "bar",
        "input", "timer", "transform", "use", "include", "fixed", "side", NULL
    };
    for (int i = 0; kw[i]; i++) if (strcmp(id, kw[i]) == 0) return 1;
    return 0;
}

static int parse_say_line(const char *work, char **speaker, char **source, char **anchor) {
    const char *p = skip_ws(work);
    if (*p == '"' || *p == '\'') {
        char *s1; const char *after = parse_string(p, &s1);
        if (!after) return 0;
        const char *a = skip_ws(after);
        if (*a == ':') { free(s1); return 0; }
        if (*a == '"' || *a == '\'') {
            char *s2; const char *after2 = parse_string(a, &s2);
            if (!after2) { free(s1); return 0; }
            *speaker = s1; *source = s2;
            size_t alen = (size_t)(a - work);
            char *anc = malloc(alen + 1); memcpy(anc, work, alen); anc[alen]=0;
            *anchor = anc;
            return 1;
        }
        if (*a == 0) {
            *speaker = dup_str(""); *source = s1;
            *anchor = dup_str("");
            return 1;
        }
        free(s1); return 0;
    }
    if (is_ident_start((unsigned char)*p)) {
        const char *id_start = p;
        while (*p && (is_ident_cont((unsigned char)*p) || *p == '.')) p++;
        size_t idlen = (size_t)(p - id_start);
        char *id = malloc(idlen + 1); memcpy(id, id_start, idlen); id[idlen]=0;
        const char *a = skip_ws(p);
        if (*a == '"' || *a == '\'') {
            if (is_display_keyword(id)) { free(id); return 0; }
            char *s; const char *after = parse_string(a, &s);
            if (!after) { free(id); return 0; }
            *speaker = id; *source = s;
            size_t alen = (size_t)(a - work);
            char *anc = malloc(alen + 1); memcpy(anc, work, alen); anc[alen]=0;
            *anchor = anc;
            return 1;
        }
        free(id); return 0;
    }
    return 0;
}

static int is_structural_line(const char *s) {
    static const char *kw[] = {
        "label ", "menu ", "screen ", "if ", "elif ", "else:", "while ",
        "for ", "python:", "init ", "transform ", "style ", "image ", "define ",
        "default ", "$ ", "return", "jump ", "call ", "pause", "show ", "hide ",
        "scene ", "play ", "stop ", "camera ", "voice ", "narrator ", "window ", NULL
    };
    for (int i = 0; kw[i]; i++) {
        size_t k = strlen(kw[i]);
        if (strncmp(s, kw[i], k) == 0) return 1;
    }
    return 0;
}

static int scan_open_triple(const char *s) {
    int trip = 0;
    for (const char *p = s; *p; ) {
        if (!trip) {
            if (p[0] == '"' && p[1] == '"' && p[2] == '"') { trip = 1; p += 3; continue; }
            if (p[0] == '\'' && p[1] == '\'' && p[2] == '\'') { trip = 1; p += 3; continue; }
            p++;
        } else {
            if (p[0] == '"' && p[1] == '"' && p[2] == '"') { trip = 0; p += 3; continue; }
            p++;
        }
    }
    return trip;
}

static void append_entry(dialog_entry_t **arr, size_t *n, size_t *cap,
                         const char *rel, int lineno, const char *type,
                         const char *speaker, char *source, char *anchor) {
    if (*n >= *cap) { *cap = *cap ? *cap*2 : 32; *arr = realloc(*arr, *cap * sizeof(**arr)); }
    dialog_entry_t e; memset(&e, 0, sizeof(e));
    e.file = dup_str(rel); e.line = lineno;
    e.type = dup_str(type);
    e.speaker = dup_str(speaker ? speaker : "");
    e.source = source; e.target = dup_str(""); e.anchor = anchor;
    (*arr)[(*n)++] = e;
}

static void extract_file(const char *rel, const char *full, dialog_entry_t **arr, size_t *n, size_t *cap) {
    FILE *f = fopen(full, "rb");
    if (!f) return;
    char line[8192];
    int lineno = 0;
    char *acc = NULL;
    int acc_start = 0;
    int in_triple = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        strip_comment(line);
        size_t L = strlen(line);
        while (L && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = 0;
        const char *work = skip_ws(line);

        if (!in_triple) {
            if (!*work) continue;
            if (is_structural_line(work)) continue;
            if (scan_open_triple(work)) {
                in_triple = 1; acc_start = lineno;
                acc = dup_str(work);
                continue;
            }
            char *source = NULL, *anchor = NULL;
            if (parse_menu_option(work, &source, &anchor)) {
                append_entry(arr, n, cap, rel, lineno, "menu", "", source, anchor);
            } else if (parse_ui_line(work, &source, &anchor)) {
                append_entry(arr, n, cap, rel, lineno, "ui", "", source, anchor);
            } else {
                char *spk = NULL;
                if (parse_say_line(work, &spk, &source, &anchor)) {
                    append_entry(arr, n, cap, rel, lineno, "say", spk, source, anchor);
                    free(spk);
                }
            }
        } else {
            size_t la = strlen(acc), lb = strlen(work);
            char *t = malloc(la + lb + 2);
            memcpy(t, acc, la); t[la] = 0;
            strcat(t, "\n"); strcat(t, work);
            free(acc); acc = t;
            if (!scan_open_triple(acc)) {
                in_triple = 0;
                char *spk = NULL, *source = NULL, *anchor = NULL;
                if (parse_say_line(acc, &spk, &source, &anchor)) {
                    append_entry(arr, n, cap, rel, acc_start, "say", spk, source, anchor);
                    free(spk);
                } else {
                    if (source) free(source);
                    if (anchor) free(anchor);
                }
                free(acc); acc = NULL;
            }
        }
    }
    if (acc) free(acc);
    fclose(f);
}

int extract_dialogs(const char *dir, dialog_entry_t **out, size_t *count) {
    file_entry_t *files = NULL; size_t n = 0;
    int rc = collect_files(dir, &files, &n);
    if (rc != RP_OK) return rc;
    dialog_entry_t *arr = NULL; size_t cnt = 0, cap = 0;
    for (size_t i = 0; i < n; i++) {
        const char *ext = strrchr(files[i].rel_path, '.');
        if (!ext) continue;
        if (strcmp(ext, ".rpyc") == 0) {
            log_warn("跳过编译文件（无法读源码文本）: %s", files[i].rel_path);
            continue;
        }
        if (strcmp(ext, ".rpy") == 0)
            extract_file(files[i].rel_path, files[i].full_path, &arr, &cnt, &cap);
    }
    free_files(files, n);
    *out = arr; *count = cnt;
    log_info("提取可翻译文本: %zu 条", cnt);
    return RP_OK;
}

void free_dialog_entries(dialog_entry_t *arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        free(arr[i].file); free(arr[i].type); free(arr[i].speaker); free(arr[i].source);
        free(arr[i].target); free(arr[i].anchor);
    }
    free(arr);
}

static void emit_rpy_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            default: fputc(*p, f);
        }
    }
    fputc('"', f);
}

int build_translation_mod(const dialog_entry_t *arr, size_t count,
                          const char *out_dir, const char *lang) {
    char tldir[2048];
    snprintf(tldir, sizeof(tldir), "%s/tl/%s", out_dir, lang);
    mkpath(tldir);

    char **uniq = NULL; size_t uniq_n = 0, uniq_cap = 0;
    for (size_t i = 0; i < count; i++) {
        if (!arr[i].target || !arr[i].target[0]) continue;
        int found = 0;
        for (size_t j = 0; j < uniq_n; j++) if (strcmp(uniq[j], arr[i].file) == 0) { found=1; break; }
        if (!found) {
            if (uniq_n >= uniq_cap) { uniq_cap = uniq_cap?uniq_cap*2:16; uniq = realloc(uniq, uniq_cap*sizeof(char*)); }
            uniq[uniq_n++] = dup_str(arr[i].file);
        }
    }

    size_t written = 0;
    for (size_t u = 0; u < uniq_n; u++) {
        char fname[2048];
        size_t k = 0;
        for (const char *p = uniq[u]; *p; p++) fname[k++] = (*p=='/') ? '_' : *p;
        fname[k] = 0;
        char *dot = strrchr(fname, '.'); if (dot) *dot = 0;

        char outpath[4096];
        snprintf(outpath, sizeof(outpath), "%s/%s.rpy", tldir, fname);
        FILE *of = fopen(outpath, "wb");
        if (!of) { log_error("无法写入翻译文件: %s", outpath); continue; }
        fprintf(of, "# 由 Ren'Py Patcher 生成 (translate %s)\n", lang);
        /* Ren'Py 要求 translate 块有名：用文件名 base + _block 作为唯一块名 */
        fprintf(of, "translate %s %s_block:\n\n", lang, fname);
        for (size_t i = 0; i < count; i++) {
            if (strcmp(arr[i].file, uniq[u]) != 0) continue;
            if (!arr[i].target || !arr[i].target[0]) continue;
            fprintf(of, "    # %s:%d  [%s]\n", arr[i].file, arr[i].line,
                    arr[i].type ? arr[i].type : "say");
            fprintf(of, "    old "); emit_rpy_string(of, arr[i].source); fprintf(of, "\n");
            fprintf(of, "    new "); emit_rpy_string(of, arr[i].target); fprintf(of, "\n\n");
            written++;
        }
        fclose(of);
        log_info("生成翻译文件: %s (%s)", outpath, uniq[u]);
    }
    for (size_t j = 0; j < uniq_n; j++) free(uniq[j]);
    free(uniq);
    log_info("翻译 mod 生成完成: %s (%zu 条译文)", tldir, written);
    return RP_OK;
}
