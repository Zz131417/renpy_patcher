/* translate/rpy_dialog.h —— Ren'Py 对话行提取 / 回填（M8 翻译流水线第一版：对话）
 *
 * 提取范围（第一版）：
 *   - 说话人 + 字符串:  e.g.  "角色" "文本"   /  m "文本"   /  e "多行文本"
 *   - 裸三引号对话:     """文本"""   （无说话人）
 * 暂不包含：menu 菜单、界面(screen)文本、py 表达式字符串。
 */
#ifndef RENPY_PATCHER_RPY_DIALOG_H
#define RENPY_PATCHER_RPY_DIALOG_H

#include <stddef.h>
#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 一条被提取的对话记录 */
typedef struct dialog_entry {
    char *file;        /* 来源 rpy 相对路径 */
    int   line;        /* 行号（1 基） */
    char *speaker;     /* 说话人标签（可能为空字符串 ""） */
    char *source;      /* 原文（已去引号/去转义） */
    char *target;      /* 译文（初始为空，人工/AI 填充） */
    char *anchor;      /* 在文件中的定位锚点：行首到字符串起始，用于回填（保留缩进/说话人） */
} dialog_entry_t;

/* 提取目录（递归）下所有 .rpy 的对话，返回条目数，*out 指向分配数组（需 free_dialog_entries） */
int extract_dialogs(const char *dir, dialog_entry_t **out, size_t *count);

/* 释放 extract_dialogs 的结果 */
void free_dialog_entries(dialog_entry_t *arr, size_t count);

/* 导出为 JSON（UTF-8）：[{file,line,speaker,source,target}, ...]，target 初始为空串 */
int export_dialogs_json(const dialog_entry_t *arr, size_t count, const char *json_path);

/* 从 JSON 读回条目，重建完整数组（file/line/speaker/source/target） */
int parse_dialogs_json(const char *json_path, dialog_entry_t **out, size_t *count);

/* 生成 Ren'Py 翻译 mod 目录：把 arr 写到 <out_dir>/tl/<lang>/ 下，
 * 按原文件拆分成 translate <lang>: / old "原文" / new "译文" 块。
 * 仅生成有译文的条目（target 非空），target 为空则跳过该行（保留原文）。 */
int build_translation_mod(const dialog_entry_t *arr, size_t count,
                          const char *out_dir, const char *lang);

#ifdef __cplusplus
}
#endif
#endif
