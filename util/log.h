#ifndef RP_LOG_H
#define RP_LOG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/* 日志级别（数值越小越详细） */
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

/* 初始化日志。file_path 为 NULL 时仅输出到 stderr。
 * 非 NULL 时同时追加写入该文件（带时间戳与级别），便于排错。 */
void log_init(const char *file_path);
/* 设置最低输出级别（低于该级别的日志被忽略）。默认 LOG_INFO。 */
void log_set_level(int level);
int  log_get_level(void);
/* 设置模块标签（如 "main" / "rpa"），可选，输出时显示为 [tag]。 */
void log_set_tag(const char *tag);

/* 格式化写日志（用法同 printf） */
void log_printf(int level, const char *fmt, ...);

/* 设置 / 获取最近一条错误详情（log_error 会自动写入 g_last_err） */
void rp_set_error(const char *fmt, ...);
const char *rp_last_error(void);
/* 错误码 -> 中文描述（供 GUI 显示详细报错） */
const char *rp_strerror(int code);

#define log_debug(...) log_printf(LOG_DEBUG, __VA_ARGS__)
#define log_info(...)  log_printf(LOG_INFO,  __VA_ARGS__)
#define log_warn(...)  log_printf(LOG_WARN,  __VA_ARGS__)
#define log_error(...) log_printf(LOG_ERROR, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif /* RP_LOG_H */
