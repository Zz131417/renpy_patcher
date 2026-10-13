/* translate/json_io.h —— 对话条目的 JSON 序列化 / 反序列化（最小自研，无第三方依赖）
 *
 * 格式（UTF-8）：
 *   [
 *     {"file":"...","line":N,"speaker":"...","source":"...","target":"..."},
 *     ...
 *   ]
 * 仅用于本项目内部交换；字符串转义支持 \" \\ \/ \b \f \n \r \t \uXXXX。
 */
#ifndef RENPY_PATCHER_JSON_IO_H
#define RENPY_PATCHER_JSON_IO_H

#include <stddef.h>
#include "translate/rpy_dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 写 JSON（UTF-8 原文，仅转义必要控制字符） */
int write_dialogs_json(const dialog_entry_t *arr, size_t count, const char *path);

/* 读 JSON，重建条目数组（file/line/speaker/source/target） */
int read_dialogs_json(const char *path, dialog_entry_t **out, size_t *count);

#ifdef __cplusplus
}
#endif
#endif
