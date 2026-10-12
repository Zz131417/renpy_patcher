/* inject/hot/py_bridge.h —— 在目标进程内定位 CPython 并解析导出符号 */
#ifndef RENPY_PATCHER_PY_BRIDGE_H
#define RENPY_PATCHER_PY_BRIDGE_H

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>

typedef struct py_api {
    void       *py_base;          /* python3X.dll 基址 */
    char        version[16];      /* 如 "3.9" */
    int  (WINAPI *PyRun_SimpleString)(const char*);
    void* (WINAPI *PyGILState_Ensure)(void);   /* 参数为 PyGILState_STATE */
    void  (WINAPI *PyGILState_Release)(void*);
    void* (WINAPI *PyImport_AddModule)(const char*);
    void* (WINAPI *PyImport_GetModuleDict)(void);
    int   (WINAPI *Py_IsInitialized)(void);
} py_api_t;

#else
typedef struct py_api { int dummy; } py_api_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 在目标进程内调用：枚举模块找 python3*.dll，解析导出符号。
 * 成功返回 RP_OK；找不到返回 RP_ERR_NOTSUP。 */
int py_resolve(py_api_t *api);

/* 便捷：确保持有 GIL 并运行一段 python 代码（需 Py_IsInitialized 为真）。
 * 成功返回 RP_OK。 */
int py_run_code(py_api_t *api, const char *code);

#ifdef __cplusplus
}
#endif
#endif
