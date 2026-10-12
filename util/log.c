/* util/log.c —— 见 util/log.h
 *
 * 极简日志：时间戳 + [级别] + [tag] + 消息，同时输出到 stderr 与可选文件。
 * 无第三方依赖；单线程 CLI 场景足够。
 */
#include "util/log.h"
#include <time.h>
#include <stdarg.h>
#include <string.h>

static FILE *g_logf  = NULL;
static int   g_level = LOG_INFO;
static char  g_tag[64] = "";   /* 已含方括号，如 "[main]" 或空 */

void log_set_tag(const char *tag) {
    if (tag && *tag) snprintf(g_tag, sizeof(g_tag), "[%s]", tag);
    else g_tag[0] = '\0';
}

void log_set_level(int level) { g_level = level; }
int  log_get_level(void)      { return g_level; }

void log_init(const char *file_path) {
    if (g_logf) { fclose(g_logf); g_logf = NULL; }
    if (file_path) {
        g_logf = fopen(file_path, "a");
        if (!g_logf) perror("log_init fopen");   /* 文件打不开不致命，仅 stderr */
    }
}

static const char *lvl_name(int l) {
    switch (l) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default:        return "?????";
    }
}

void log_printf(int level, const char *fmt, ...) {
    if (level < g_level) return;

    char ts[32]; ts[0] = '\0';
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (tm) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

    char msg[2048];
    va_list ap; va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    char hdr[256];
    snprintf(hdr, sizeof(hdr), "[%s]%s[%s] ", lvl_name(level), g_tag, ts);

    fprintf(stderr, "%s%s\n", hdr, msg);
    if (g_logf) {
        fprintf(g_logf, "%s%s\n", hdr, msg);
        fflush(g_logf);
    }
}
