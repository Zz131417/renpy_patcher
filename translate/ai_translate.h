/* translate/ai_translate.h —— AI 翻译后端（OpenAI 兼容 Chat Completions）
 *
 * 支持任意兼容 OpenAI Chat Completions 的服务（OpenAI / DeepSeek / 通义 / 本地 llama.cpp 等）。
 * 配置优先级：函数参数 > 环境变量 > 默认值。
 *   RP_AI_BASE_URL   默认 https://api.openai.com/v1
 *   RP_AI_API_KEY    必填（无则不可翻译）
 *   RP_AI_MODEL      默认 gpt-4o-mini
 */
#ifndef RENPY_PATCHER_AI_TRANSLATE_H
#define RENPY_PATCHER_AI_TRANSLATE_H

#include <stddef.h>
#include "core/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 一次性翻译一批文本（批量调用以提升效率），把结果写回 targets（长度 = count）。
 * 返回 RP_OK 成功（部分失败也会尽力填充，无法翻译的保留原文）。 */
int ai_translate_batch(const char **sources, char **targets, size_t count,
                       const char *from_lang, const char *to_lang,
                       const char *base_url, const char *api_key, const char *model);

#ifdef __cplusplus
}
#endif
#endif
