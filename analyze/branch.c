/* analyze/branch.c —— Ren'Py 聊天/选择分支解析与导图生成
 *
 * 解析游戏脚本（.rpy）里的 label 与 menu 选择分支，构建「分支图」并导出
 * JSON / Mermaid / DOT / 文本树，便于快速看清视觉小说 / 聊天 AVG 的选择走向。
 */
#include "analyze/branch.h"
#include "core/plugin.h"
#include "util/dir_walk.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

static void strip_comment(char *line) {
    int in_s = 0, in_d = 0;
    for (char *p = line; *p; p++) {
        if (*p == '\'' && !in_d) in_s = !in_s;
        else if (*p == '"' && !in_s) in_d = !in_d;
        else if (*p == '#' && !in_s && !in_d) { *p = 0; break; }
    }
}

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* 解析 Ren'Py 字符串字面量（' " 或三引号），返回内容（需 free），*after 指后续位置 */
static const char *parse_string(const char *p, char **out, const char **after) {
    char q = *p;
    int triple = 0;
    if (q == '"' && p[1] == '"' && p[2] == '"') { triple = 1; p += 3; }
    else if (q == '\'' && p[1] == '\'' && p[2] == '\'') { triple = 1; p += 3; }
    else if (q == '"' || q == '\'') { p += 1; }
    else return NULL;

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
                    default: add=n; break;
                }
                if (len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=add;
                p += 2; continue;
            }
            if (*p == q) { buf[len]=0; *out=buf; if(after)*after=p+1; return p+1; }
            if (len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=*p++;
        } else {
            if (*p == '"' && p[1] == '"' && p[2] == '"') { buf[len]=0; *out=buf; if(after)*after=p+3; return p+3; }
            if (*p == '\'' && p[1] == '\'' && p[2] == '\'') { buf[len]=0; *out=buf; if(after)*after=p+3; return p+3; }
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

static int indent_of(const char *line) {
    int n = 0;
    for (const char *p = line; *p == ' ' || *p == '\t'; p++) n += (*p == '\t') ? 4 : 1;
    return n;
}

static int scan_open_triple(const char *s) {
    int trip = 0;
    for (const char *p = s; *p; ) {
        if (!trip) {
            if (p[0]=='"'&&p[1]=='"'&&p[2]=='"'){trip=1;p+=3;continue;}
            if (p[0]=='\''&&p[1]=='\''&&p[2]=='\''){trip=1;p+=3;continue;}
            p++;
        } else {
            if (p[0]=='"'&&p[1]=='"'&&p[2]=='"'){trip=0;p+=3;continue;}
            if (p[0]=='\''&&p[1]=='\''&&p[2]=='\''){trip=0;p+=3;continue;}
            p++;
        }
    }
    return trip;
}

static int is_ident_start(int c){ return isalpha((unsigned char)c)||c=='_'||c>127; }
static int is_ident_cont(int c){ return isalnum((unsigned char)c)||c=='_'||c>127; }

/* 取标识符（从 p 起），返回长度；不分配 */
static int take_ident(const char *p, char *buf, size_t bufsz) {
    if (!is_ident_start((unsigned char)*p)) return 0;
    size_t n = 0;
    while (*p && is_ident_cont((unsigned char)*p) && n + 1 < bufsz) buf[n++] = *p++;
    buf[n] = 0;
    return (int)n;
}

/* ---------- 图构建 ---------- */

static int find_or_add_label(branch_graph_t *g, const char *name, const char *file, int line) {
    for (size_t i = 0; i < g->n_labels; i++)
        if (strcmp(g->labels[i].name, name) == 0) return g->labels[i].id;
    if (g->n_labels >= g->cap_labels) {
        g->cap_labels = g->cap_labels ? g->cap_labels*2 : 16;
        g->labels = realloc(g->labels, g->cap_labels * sizeof(*g->labels));
    }
    branch_label_t *L = &g->labels[g->n_labels];
    L->name = dup_str(name); L->file = dup_str(file); L->line = line; L->id = (int)g->n_labels;
    return (int)(g->n_labels++);
}

static int resolve_label_id(branch_graph_t *g, const char *name) {
    if (!name) return -1;
    for (size_t i = 0; i < g->n_labels; i++)
        if (strcmp(g->labels[i].name, name) == 0) return g->labels[i].id;
    return -1;
}

static int add_edge(branch_graph_t *g, int from, int to, const char *kind, const char *text) {
    if (g->n_edges >= g->cap_edges) {
        g->cap_edges = g->cap_edges ? g->cap_edges*2 : 32;
        g->edges = realloc(g->edges, g->cap_edges * sizeof(*g->edges));
    }
    branch_edge_t *e = &g->edges[g->n_edges++];
    e->from = from; e->to = to;
    strncpy(e->kind, kind, sizeof(e->kind)-1); e->kind[sizeof(e->kind)-1]=0;
    e->text = text ? dup_str(text) : dup_str("");
    e->to_name = NULL;
    return (int)(g->n_edges - 1);
}

/* 判断是否为 label 定义行（strip 后 "label NAME:" 或 "label NAME(...):"） */
static int parse_label_def(const char *work, char *namebuf, size_t bufsz) {
    if (strncmp(work, "label ", 6) != 0) return 0;
    const char *p = skip_ws(work + 5);
    int n = take_ident(p, namebuf, bufsz);
    if (n == 0) return 0;
    p += n;
    p = skip_ws(p);
    if (*p == ':' || *p == '(') return 1;
    return 0;
}

/* 判断是否为 menu 块开始行 */
static int is_menu_start(const char *work) {
    if (strncmp(work, "menu", 4) != 0) return 0;
    char c = work[4];
    if (!(c == ' ' || c == '\t' || c == ':')) return 0;
    /* 其后需含 ':'（menu 名可能有缩进） */
    const char *colon = strchr(work + 4, ':');
    return colon != NULL;
}

/* 解析 menu 选项行：strip 后字符串字面量 + (':' | ' if ') */
static int parse_menu_option(const char *work, char **out_text) {
    const char *p = skip_ws(work);
    if (*p != '"' && *p != '\'') return 0;
    char *s; const char *after;
    if (!parse_string(p, &s, &after)) return 0;
    const char *a = skip_ws(after);
    if (*a == ':' || (a[0] == 'i' && a[1] == 'f' && (a[2]==' '||a[2]=='\t'||a[2]=='('))) {
        *out_text = s; return 1;
    }
    free(s);
    return 0;
}

/* 解析 jump/call/return，返回 kind（"jump"/"call"/"return"/NULL），target 写入 tbuf */
static const char *parse_control(const char *work, char *tbuf, size_t bufsz) {
    if (strncmp(work, "jump", 4) == 0 && (work[4]==' '||work[4]=='\t')) {
        const char *p = skip_ws(work + 4);
        if (take_ident(p, tbuf, bufsz) > 0) return "jump";
        return NULL;
    }
    if (strncmp(work, "call", 4) == 0 && (work[4]==' '||work[4]=='\t')) {
        const char *p = skip_ws(work + 4);
        if (take_ident(p, tbuf, bufsz) > 0) return "call";
        return NULL;
    }
    if (strcmp(work, "return") == 0 || strncmp(work, "return ", 7) == 0) return "return";
    return NULL;
}

static void analyze_file(branch_graph_t *g, const char *rel, const char *full) {
    FILE *f = fopen(full, "rb");
    if (!f) return;
    char line[8192];
    int lineno = 0;
    int in_triple = 0;
    int cur_label = -1;
    int menu_depth = -1;          /* 当前 menu 块缩进；-1 表示不在 menu 内 */
    int opt_active = 0;           /* 是否正在收集某选项的目标跳转 */
    int opt_depth = 0;            /* 选项行缩进 */
    int pending_opt_edge = -1;    /* 该选项已加入的 edge 索引 */
    char opt_target[256]; (void)opt_target; opt_target[0]=0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        strip_comment(line);
        size_t L = strlen(line);
        while (L && (line[L-1]=='\n'||line[L-1]=='\r')) line[--L]=0;
        const char *work = skip_ws(line);

        if (!in_triple) {
            if (scan_open_triple(work)) { in_triple = 1; continue; }
            if (!*work) continue;
        } else {
            if (!scan_open_triple(work)) in_triple = 0;
            continue; /* 多行字符串内部不解析 */
        }

        int ind = indent_of(line);

        /* 结束 menu 块（更深语句回到 menu 同级或更浅） */
        if (menu_depth >= 0 && ind <= menu_depth) {
            menu_depth = -1; opt_active = 0; pending_opt_edge = -1;
        }
        /* 选项体结束（遇到与选项同级或更浅的语句） */
        if (opt_active && ind <= opt_depth) {
            opt_active = 0; pending_opt_edge = -1;
        }

        /* label 定义 */
        char lname[256];
        if (parse_label_def(work, lname, sizeof(lname))) {
            cur_label = find_or_add_label(g, lname, rel, lineno);
            menu_depth = -1; opt_active = 0; pending_opt_edge = -1;
            continue;
        }

        /* menu 块开始 */
        if (is_menu_start(work)) {
            menu_depth = ind;
            opt_active = 0; pending_opt_edge = -1;
            continue;
        }

        /* menu 选项（在 menu 块内、缩进比 menu 更深） */
        if (menu_depth >= 0 && ind > menu_depth && cur_label >= 0) {
            char *otext = NULL;
            if (parse_menu_option(work, &otext)) {
                int e = add_edge(g, cur_label, -1, "menu", otext);
                free(otext);
                opt_active = 1; opt_depth = ind; pending_opt_edge = e;
                opt_target[0] = 0;
                continue;
            }
        }

        /* 在选项体内：寻找第一条跳转/返回，回填 pending edge */
        if (opt_active && pending_opt_edge >= 0) {
            char tbuf[256];
            const char *ck = parse_control(work, tbuf, sizeof(tbuf));
            if (ck) {
                branch_edge_t *pe = &g->edges[pending_opt_edge];
                if (strcmp(ck, "return") != 0) pe->to_name = dup_str(tbuf);
                opt_active = 0; pending_opt_edge = -1;
            }
            /* 选项体内若不是跳转，继续等待（直到选项体结束） */
            continue;
        }

        /* 顶层跳转/返回（不在 menu 选项体内） */
        if (!opt_active && cur_label >= 0) {
            char tbuf[256];
            const char *ck = parse_control(work, tbuf, sizeof(tbuf));
            if (ck) {
                int idx = add_edge(g, cur_label, -1, ck, "");
                if (strcmp(ck, "return") != 0)
                    g->edges[idx].to_name = dup_str(tbuf);
            }
        }
    }
    fclose(f);
}

int analyze_branches(const char *dir, branch_graph_t *g) {
    memset(g, 0, sizeof(*g));
    g->start_id = -1;
    file_entry_t *files = NULL; size_t n = 0;
    int rc = collect_files(dir, &files, &n);
    if (rc != RP_OK) return rc;
    for (size_t i = 0; i < n; i++) {
        const char *ext = strrchr(files[i].rel_path, '.');
        if (!ext) continue;
        if (strcmp(ext, ".rpy") == 0)
            analyze_file(g, files[i].rel_path, files[i].full_path);
        else if (strcmp(ext, ".rpyc") == 0)
            log_warn("跳过编译文件（无源码）: %s", files[i].rel_path);
    }
    free_files(files, n);

    /* 第二遍：把边上的 to_name 解析为 label id（支持前向引用 / 跨文件引用） */
    for (size_t i = 0; i < g->n_edges; i++) {
        if (g->edges[i].to_name)
            g->edges[i].to = resolve_label_id(g, g->edges[i].to_name);
    }

    /* 定位 start label */
    for (size_t i = 0; i < g->n_labels; i++)
        if (strcmp(g->labels[i].name, "start") == 0) { g->start_id = (int)i; break; }

    log_info("分支解析: %zu 个 label, %zu 条分支边", g->n_labels, g->n_edges);
    return RP_OK;
}

void branch_free(branch_graph_t *g) {
    if (!g) return;
    for (size_t i = 0; i < g->n_labels; i++) { free(g->labels[i].name); free(g->labels[i].file); }
    for (size_t i = 0; i < g->n_edges; i++) { free(g->edges[i].text); free(g->edges[i].to_name); }
    free(g->labels); free(g->edges);
    memset(g, 0, sizeof(*g));
}

/* ---------- 导出 ---------- */

static FILE *open_out(const char *path) {
    if (!path || strcmp(path, "-") == 0) return stdout;
    return fopen(path, "wb");
}

static void esc_json(FILE *f, const char *s) {
    fputc('"', f);
    for (const char *p = s; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"': fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            default: if (c < 0x20) fprintf(f, "\\u%04x", c); else fputc(c, f);
        }
    }
    fputc('"', f);
}

int branch_export_json(const branch_graph_t *g, const char *path) {
    FILE *f = open_out(path);
    if (!f) return RP_ERR_IO;
    fputs("{\n  \"start\": ", f);
    if (g->start_id >= 0) esc_json(f, g->labels[g->start_id].name); else fputs("null", f);
    fputs(",\n  \"labels\": [", f);
    for (size_t i = 0; i < g->n_labels; i++) {
        if (i) fputc(',', f);
        fprintf(f, "\n    {\"id\":%d,\"name\":", g->labels[i].id);
        esc_json(f, g->labels[i].name);
        fputs(",\"file\":", f); esc_json(f, g->labels[i].file);
        fprintf(f, ",\"line\":%d}", g->labels[i].line);
    }
    fputs("\n  ],\n  \"edges\": [", f);
    for (size_t i = 0; i < g->n_edges; i++) {
        if (i) fputc(',', f);
        fprintf(f, "\n    {\"from\":%d,\"to\":%d,\"kind\":\"%s\",\"text\":",
                g->edges[i].from, g->edges[i].to, g->edges[i].kind);
        esc_json(f, g->edges[i].text);
        fputc('}', f);
    }
    fputs("\n  ]\n}\n", f);
    if (f != stdout) fclose(f);
    return RP_OK;
}

static const char *label_name(const branch_graph_t *g, int id) {
    if (id < 0 || (size_t)id >= g->n_labels) return NULL;
    return g->labels[id].name;
}

int branch_export_mermaid(const branch_graph_t *g, const char *path) {
    FILE *f = open_out(path);
    if (!f) return RP_ERR_IO;
    fputs("flowchart TD\n", f);
    /* 节点 */
    for (size_t i = 0; i < g->n_labels; i++)
        fprintf(f, "  L%d[\"%s\"]\n", g->labels[i].id, g->labels[i].name);
    fputs("  END[(\"结束/就地继续\")]\n", f);
    /* 边 */
    for (size_t i = 0; i < g->n_edges; i++) {
        const branch_edge_t *e = &g->edges[i];
        const char *tn = label_name(g, e->to);
        if (e->kind[0] == 'm' && e->text[0]) {
            if (tn) fprintf(f, "  L%d -->|\"%s\"| L%d\n", e->from, e->text, e->to);
            else    fprintf(f, "  L%d -->|\"%s\"| END\n", e->from, e->text);
        } else if (tn) {
            fprintf(f, "  L%d -->|%s| L%d\n", e->from, e->kind, e->to);
        } else {
            fprintf(f, "  L%d -->|%s| END\n", e->from, e->kind);
        }
    }
    if (f != stdout) fclose(f);
    return RP_OK;
}

int branch_export_dot(const branch_graph_t *g, const char *path) {
    FILE *f = open_out(path);
    if (!f) return RP_ERR_IO;
    fputs("digraph branches {\n  rankdir=TB;\n", f);
    for (size_t i = 0; i < g->n_labels; i++)
        fprintf(f, "  L%d [label=\"%s\"];\n", g->labels[i].id, g->labels[i].name);
    fprintf(f, "  END [label=\"结束/就地继续\", shape=doublecircle];\n");
    for (size_t i = 0; i < g->n_edges; i++) {
        const branch_edge_t *e = &g->edges[i];
        const char *tn = label_name(g, e->to);
        if (e->kind[0] == 'm' && e->text[0]) {
            if (tn) fprintf(f, "  L%d -> L%d [label=\"%s\"];\n", e->from, e->to, e->text);
            else    fprintf(f, "  L%d -> END [label=\"%s\"];\n", e->from, e->text);
        } else if (tn) {
            fprintf(f, "  L%d -> L%d [label=\"%s\"];\n", e->from, e->to, e->kind);
        } else {
            fprintf(f, "  L%d -> END [label=\"%s\"];\n", e->from, e->kind);
        }
    }
    fputs("}\n", f);
    if (f != stdout) fclose(f);
    return RP_OK;
}

/* 文本树：从 start（无则首 label）DFS；menu 边展开选项，jump/call 边连向目标 */
static void dfs_tree(const branch_graph_t *g, int id, int depth, char *seen, FILE *f) {
    if (id < 0 || (size_t)id >= g->n_labels || seen[id]) {
        for (int d = 0; d < depth; d++) fputs("  ", f);
        fputs("(循环/已访问)\n", f);
        return;
    }
    seen[id] = 1;
    for (int d = 0; d < depth; d++) fputs("  ", f);
    fprintf(f, "▣ %s\n", g->labels[id].name);

    for (size_t i = 0; i < g->n_edges; i++) {
        const branch_edge_t *e = &g->edges[i];
        if (e->from != id) continue;
        for (int d = 0; d < depth + 1; d++) fputs("  ", f);
        if (e->kind[0] == 'm' && e->text[0]) {
            fprintf(f, "├─ [选项] %s", e->text);
            const char *tn = label_name(g, e->to);
            if (tn) fprintf(f, "  → %s\n", tn);
            else fprintf(f, "  → (结束/就地继续)\n");
            if (tn) dfs_tree(g, e->to, depth + 2, seen, f);
        } else if (e->kind[0] == 'm') {
            /* menu 边但无文本（理论不应有） */
            const char *tn = label_name(g, e->to);
            if (tn) { fprintf(f, "├─ [分支] → %s\n", tn); dfs_tree(g, e->to, depth + 1, seen, f); }
        } else {
            const char *tn = label_name(g, e->to);
            if (tn) { fprintf(f, "├─ (%s) → %s\n", e->kind, tn); dfs_tree(g, e->to, depth + 1, seen, f); }
            else fprintf(f, "├─ (%s) → (结束/就地继续)\n", e->kind);
        }
    }
}

int branch_export_tree(const branch_graph_t *g, const char *path) {
    FILE *f = open_out(path);
    if (!f) return RP_ERR_IO;
    int root = g->start_id >= 0 ? g->start_id : (g->n_labels ? 0 : -1);
    if (root < 0) { fputs("(无 label)\n", f); if (f!=stdout) fclose(f); return RP_OK; }
    char *seen = calloc(g->n_labels ? g->n_labels : 1, 1);
    fprintf(f, "聊天分支导图（从 %s 起）:\n", g->labels[root].name);
    dfs_tree(g, root, 0, seen, f);
    free(seen);
    if (f != stdout) fclose(f);
    return RP_OK;
}

int branch_export(const branch_graph_t *g, const char *fmt, const char *path) {
    if (!fmt || !*fmt || strcmp(fmt, "mermaid") == 0) return branch_export_mermaid(g, path);
    if (strcmp(fmt, "json") == 0) return branch_export_json(g, path);
    if (strcmp(fmt, "dot") == 0) return branch_export_dot(g, path);
    if (strcmp(fmt, "tree") == 0) return branch_export_tree(g, path);
    log_warn("未知导图格式 '%s'，回退到 mermaid", fmt);
    return branch_export_mermaid(g, path);
}
