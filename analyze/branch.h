/* analyze/branch.h —— Ren'Py 聊天/选择分支解析与导图生成
 *
 * 解析游戏脚本（.rpy）中的 label 与 menu 选择分支，构建「分支图」：
 *   - 节点：所有 label（及菜单选项）
 *   - 边：menu 选项（带选项文本，指向其跳转目标 label）、jump/call 跳转边
 * 导出格式：JSON（供程序/界面）、Mermaid（flowchart，可渲染）、DOT（graphviz）、文本树（从 start DFS）。
 *
 * 用途：快速看清视觉小说 / 聊天 AVG 的选择分支走向，生成「聊天分支导图」。
 */
#ifndef RENPY_PATCHER_BRANCH_H
#define RENPY_PATCHER_BRANCH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct branch_label {
    char *name;     /* label 名 */
    char *file;     /* 来源文件 */
    int   line;     /* 行号（1 基） */
    int   id;       /* 内部编号 */
} branch_label_t;

typedef struct branch_edge {
    int   from;     /* 源 label 编号 */
    int   to;       /* 目标 label 编号（-1 表示结束/就地继续/return） */
    char  kind[12]; /* "menu" / "jump" / "call" / "return" */
    char *text;     /* menu 选项文本（menu 边填写） */
    char *to_name;  /* 目标 label 名（解析阶段先存名，全部 label 收集后再解析为 id；NULL 表示结束/就地继续） */
} branch_edge_t;

typedef struct branch_graph {
    branch_label_t *labels; size_t n_labels, cap_labels;
    branch_edge_t  *edges;  size_t n_edges,  cap_edges;
    int start_id;           /* start label 编号，-1 表示未找到 */
} branch_graph_t;

/* 解析目录下所有 .rpy，构建分支图。返回 RP_OK / 错误码。结果需 branch_free 释放。 */
int analyze_branches(const char *dir, branch_graph_t *g);

/* 释放 analyze_branches 的结果 */
void branch_free(branch_graph_t *g);

/* 导出为 JSON 文本（写入 path；utf-8）。返回 RP_OK / 错误码 */
int branch_export_json(const branch_graph_t *g, const char *path);

/* 导出为 Mermaid flowchart 文本 */
int branch_export_mermaid(const branch_graph_t *g, const char *path);

/* 导出为 Graphviz DOT 文本 */
int branch_export_dot(const branch_graph_t *g, const char *path);

/* 导出为文本树（从 start label DFS；无 start 则从第一个 label 起）。
 * 仅展示 menu 分支与跳转链。 */
int branch_export_tree(const branch_graph_t *g, const char *path);

/* 便捷：按 fmt 选择导出（"json"|"mermaid"|"dot"|"tree"，默认 mermaid）。
 * 返回 RP_OK / 错误码。 */
int branch_export(const branch_graph_t *g, const char *fmt, const char *path);

#ifdef __cplusplus
}
#endif
#endif
