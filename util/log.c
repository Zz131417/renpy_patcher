/* util/log.c —— 见 util/log.h
 *
 * 极简日志：时间戳 + [级别] + [tag] + 消息，同时输出到 stderr 与可选文件。
 * 无第三方依赖；单线程 CLI 场景足够。
 */
#include "util/log.h"
#include "core/plugin.h"   /* RP_OK 等错误码，供 rp_strerror 使用 */
#include <time.h>
#include <stdarg.h>
#include <string.h>

static FILE *g_logf  = NULL;
static int   g_level = LOG_INFO;
static char  g_tag[64] = "";
static char  g_last_err[1024] = "";   /* 最近一条错误详情，供 GUI 取回 */
static rp_log_callback_t g_log_cb = NULL;

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

    if (level >= LOG_ERROR) {
        strncpy(g_last_err, msg, sizeof(g_last_err) - 1);
        g_last_err[sizeof(g_last_err) - 1] = 0;
    }
    if (g_log_cb) g_log_cb(level, msg);
}

void log_set_callback(rp_log_callback_t cb) { g_log_cb = cb; }

/* ---- 最后错误 / 错误码翻译（供 GUI 显示详细报错）---- */
void rp_set_error(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_last_err, sizeof(g_last_err), fmt, ap);
    va_end(ap);
}
const char *rp_last_error(void) { return g_last_err; }

const char *rp_strerror(int code)
{
    switch (code) {
        case RP_OK:         return "成功";
        case RP_ERR_NOMEM:  return "内存不足";
        case RP_ERR_IO:     return "I/O 错误（文件打开/读取/写入失败）";
        case RP_ERR_FORMAT: return "文件格式错误（不是有效的资源包或数据）";
        case RP_ERR_NOTSUP: return "不支持的操作";
        case RP_ERR_GENERIC:return "通用错误";
        case RP_ERR_PLUGIN: return "插件加载或执行错误";
        default:            return "未知错误";
    }
}
