/* translate/ai_translate.c —— AI 翻译后端（OpenAI 兼容 Chat Completions）
 *
 * 用系统 curl 调用 HTTP API（CI 与大多数环境自带 curl，避免引入 C 网络库）。
 * 单批提交多条文本，要求模型以 JSON 数组返回译文，降低调用次数。
 */
#define _POSIX_C_SOURCE 200809L
#define _POSIX_C_SOURCE 200809L
#include "translate/ai_translate.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static long mypid(void) {
#ifdef _WIN32
    return (long)GetCurrentProcessId();
#else
    return (long)getpid();
#endif
}
#include <unistd.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <windows.h>
#endif

/* 读取文件全部内容到 malloc 缓冲（末尾补 \0），返回长度；失败返回 -1 */
static long read_file(const char *path, char **buf) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(sz + 1);
    if (!b) { fclose(f); return -1; }
    size_t r = fread(b, 1, sz, f); fclose(f);
    b[r] = 0;
    *buf = b;
    return (long)r;
}

/* 写字符串到文件 */
static int write_file(const char *path, const char *s) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fputs(s, f); fclose(f);
    return 0;
}

/* 简单 str 替换（就地不够则 malloc） */
static char *json_escape(const char *s) {
    /* 估算 */
    size_t cap = strlen(s) * 2 + 8;
    char *out = malloc(cap);
    size_t j = 0;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (c == '"' || c == '\\') { out[j++] = '\\'; out[j++] = c; }
        else if (c == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
        else if (c == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else if (c == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else out[j++] = c;
    }
    out[j] = 0;
    return out;
}

/* 用 curl 发送 POST，返回响应体（malloc，需 free）；失败返回 NULL */
static char *curl_post(const char *url, const char *body, const char *api_key) {
    char hdr_auth[1024], hdr_ct[64], hdr_ver[64];
    snprintf(hdr_auth, sizeof(hdr_auth), "Authorization: Bearer %s", api_key ? api_key : "");
    snprintf(hdr_ct, sizeof(hdr_ct), "Content-Type: application/json");
    snprintf(hdr_ver, sizeof(hdr_ver), "api-version: 2024-01-01-preview");

    char req_path[2048];
    snprintf(req_path, sizeof(req_path), "rp_ai_req_%ld.tmp", mypid());
    char resp_path[2048];
    snprintf(resp_path, sizeof(resp_path), "rp_ai_resp_%ld.tmp", mypid());

    if (write_file(req_path, body) != 0) return NULL;

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "curl -sS -m 120 -o \"%s\" -w \"%%{http_code}\" -X POST \"%s\" "
        "-H \"%s\" -H \"%s\" -H \"%s\" --data-binary @\"%s\" 2>/dev/null",
        resp_path, url, hdr_auth, hdr_ct, hdr_ver, req_path);

    FILE *pf = popen(cmd, "r");
    char code[16] = {0};
    if (pf) { fgets(code, sizeof(code), pf); pclose(pf); }
    remove(req_path);

    char *resp = NULL;
    long rsz = read_file(resp_path, &resp);
    remove(resp_path);
    if (rsz < 0) return NULL;
    (void)code;
    return resp;
}

/* 解析模型返回的 JSON 数组（["a","b",...]），写入 targets（按序）；解析失败保留 NULL */
static void parse_json_array(const char *s, char **targets, size_t count) {
    /* 找第一个 '[' */
    const char *p = strchr(s, '[');
    if (!p) return;
    p++;
    size_t idx = 0;
    while (*p && idx < count) {
        /* 跳过空白与逗号 */
        while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',')) p++;
        if (*p == ']') break;
        if (*p != '"') break;  /* 格式不符 */
        p++;
        size_t cap = 64, len = 0; char *buf = malloc(cap);
        while (*p) {
            if (*p == '\\' && p[1]) {
                p++; char n = *p++;
                char add = n;
                if (n == 'n') add = '\n'; else if (n == 't') add = '\t';
                else if (n == 'r') add = '\r'; else if (n == '"') add = '"'; else if (n == '\\') add = '\\';
                if (len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=add;
            } else if (*p == '"') { p++; break; }
            else { if(len+1>=cap){cap*=2;buf=realloc(buf,cap);} buf[len++]=*p++; }
        }
        buf[len] = 0;
        if (targets[idx]) free(targets[idx]);
        targets[idx] = buf;
        idx++;
    }
}

int ai_translate_batch(const char **sources, char **targets, size_t count,
                       const char *from_lang, const char *to_lang,
                       const char *base_url, const char *api_key, const char *model) {
    if (count == 0) return RP_OK;
    if (!api_key || !*api_key) {
        log_error("AI 翻译缺少 API Key（设置 RP_AI_API_KEY 环境变量或 --ai-key）");
        return RP_ERR_PLUGIN;
    }
    if (!base_url || !*base_url) base_url = "https://api.openai.com/v1";
    if (!model || !*model) model = "gpt-4o-mini";

    /* 构造 prompt：要求返回 JSON 数组，顺序对应输入 */
    /* 系统提示 */
    char sys[256];
    snprintf(sys, sizeof(sys), "You are a game localization translator. Translate the following Ren'Py game lines from %s to %s. Keep character names, code, and formatting intact. Return ONLY a JSON array of translated strings in the same order, no extra text.",
             from_lang ? from_lang : "the source language", to_lang ? to_lang : "Chinese");

    /* 构造 user content：每行一条，编号 */
    size_t cap = 256; char *user = malloc(cap);
    size_t len = 0;
    len += snprintf(user + len, cap - len, "Translate each line. Reply with a JSON array of %zu strings.\n", count);
    for (size_t i = 0; i < count; i++) {
        char tmp[32]; snprintf(tmp, sizeof(tmp), "[%zu] ", i + 1);
        /* 估算扩展 */
        size_t need = len + strlen(tmp) + strlen(sources[i]) + 2;
        if (need >= cap) { cap = need * 2; user = realloc(user, cap); }
        len += snprintf(user + len, cap - len, "%s%s\n", tmp, sources[i]);
    }

    /* 构造请求体 */
    char *sys_esc = json_escape(sys);
    char *user_esc = json_escape(user);
    size_t bcap = strlen(sys_esc) + strlen(user_esc) + strlen(model) + 512;
    char *body = malloc(bcap);
    snprintf(body, bcap,
        "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0.3}",
        model, sys_esc, user_esc);
    free(sys_esc); free(user_esc); free(user);

    char url[1024];
    snprintf(url, sizeof(url), "%s/chat/completions", base_url);

    log_info("AI 翻译请求: %zu 条 -> %s (%s)", count, url, model);
    char *resp = curl_post(url, body, api_key);
    free(body);
    if (!resp) {
        log_error("AI 翻译请求失败（curl 不可用或网络错误）");
        return RP_ERR_IO;
    }

    /* 提取 choices[0].message.content */
    char *content = NULL;
    char *mc = strstr(resp, "\"content\"");
    if (mc) {
        char *colon = strchr(mc, ':');
        if (colon) {
            /* 跳过空白与可能的引号 */
            char *p = colon + 1;
            while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
            if (*p == '"') {
                p++;
                size_t ccap = 256, clen = 0; content = malloc(ccap);
                while (*p) {
                    if (*p == '\\' && p[1]) {
                        p++; char n = *p++;
                        char add = n;
                        if (n=='n') add='\n'; else if(n=='t') add='\t'; else if(n=='r') add='\r';
                        else if(n=='"') add='"'; else if(n=='\\') add='\\';
                        if (clen+1>=ccap){ccap*=2;content=realloc(content,ccap);} content[clen++]=add;
                    } else if (*p == '"') {
                        /* 看后面是否紧跟逗号/} 或 \n 才视为结束（避免文本内引号） */
                        /* 简单：检查下一个非空白是否为 , 或 } */
                        char *q = p + 1;
                        while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') q++;
                        if (*q == ',' || *q == '}') { p++; break; }
                        else { if(clen+1>=ccap){ccap*=2;content=realloc(content,ccap);} content[clen++]='"'; p++; }
                    } else {
                        if (clen+1>=ccap){ccap*=2;content=realloc(content,ccap);} content[clen++]=*p++;
                    }
                }
                content[clen] = 0;
            }
        }
    }

    if (!content) {
        log_error("AI 翻译响应解析失败（无 content）；原始响应前 200 字: %.200s", resp);
        free(resp);
        return RP_ERR_FORMAT;
    }

    parse_json_array(content, targets, count);
    free(content);
    free(resp);
    log_info("AI 翻译完成");
    return RP_OK;
}
