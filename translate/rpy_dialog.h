/* translate/rpy_dialog.h —— Ren'Py 可翻译文本提取 / 回填
 *
 * 提取范围：
 *   - 对话(say)：说话人+字符串 / 裸字符串 / 多行三引号
 *   - 菜单(menu)：menu 选项文本（"选项": / "选项" if ...:）
 *   - 界面(ui)：screen 内显示文本（text / textbutton / button 的 text / hover / idle / tooltip 等）
 * 三种类型统一以 Ren'Py 的 old/new 字符串翻译机制回填，覆盖对话与界面/菜单文本。
 */
#ifndef RENPY_PATCHER_RPY_DIALOG_H
#define RENPY_PATCHER_RPY_DIALOG_H

#include <stddef.h>
#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 一条被提取的可翻译文本记录 */
typedef struct dialog_entry {
    char *file;        /* 来源 rpy 相对路径 */
    int   line;        /* 行号（1 基） */
    char *type;        /* 类型："say" / "menu" / "ui" */
    char *speaker;     /* 说话人标签（say 用；menu/ui 为空串 ""） */
    char *source;      /* 原文（已去引号/去转义） */
    char *target;      /* 译文（初始为空，人工/AI 填充） */
    char *anchor;      /* 在文件中的定位锚点（保留用，目前生成翻译走 old/new） */
} dialog_entry_t;

/* 提取目录（递归）下所有 .rpy 的可翻译文本，返回条目数，*out 指向分配数组（需 free_dialog_entries） */
int extract_dialogs(const char *dir, dialog_entry_t **out, size_t *count);

/* 释放 extract_dialogs 的结果 */
void free_dialog_entries(dialog_entry_t *arr, size_t count);

/* 导出为 JSON（UTF-8）：[{file,line,type,speaker,source,target}, ...]，target 初始为空串 */
int export_dialogs_json(const dialog_entry_t *arr, size_t count, const char *json_path);

/* 从 JSON 读回条目，重建完整数组（file/line/type/speaker/source/target） */
int parse_dialogs_json(const char *json_path, dialog_entry_t **out, size_t *count);

/* 生成 Ren'Py 翻译 mod 目录：把 arr 写到 <out_dir>/tl/<lang>/ 下，
 * 按原文件拆分成 translate <lang> <block>: / old "原文" / new "译文" 块。
 * 三种类型（say/menu/ui）统一用 old/new 字符串翻译，Ren'Py 会自动匹配替换。
 * 仅生成有译文的条目（target 非空），target 为空则跳过（保留原文）。 */
int build_translation_mod(const dialog_entry_t *arr, size_t count,
                          const char *out_dir, const char *lang);

#ifdef __cplusplus
}
#endif
#endif
