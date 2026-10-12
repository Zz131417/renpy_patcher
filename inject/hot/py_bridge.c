/* inject/hot/py_bridge.c —— 见 py_bridge.h */
#include "py_bridge.h"
#include "core/plugin.h"

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string.h>
#include <stdio.h>

/* 在目标进程（即当前进程，agent 被注入后运行于此）中枚举模块，
 * 找到 python3*.dll（Ren'Py 自带），返回模块句柄并输出基名。 */
static HMODULE find_python_module(char *name_out, size_t name_cap) {
    HMODULE result = NULL;
    if (name_out) name_out[0] = '\0';
    HANDLE snap = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return NULL;
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    if (Module32First(snap, &me)) {
        do {
            const char *n = me.szModule;
            if (strncmp(n, "python3", 7) == 0 ||
                (strncmp(n, "python", 6) == 0 && n[6] >= '3')) {
                result = me.hModule;
                if (name_out) {
                    strncpy(name_out, n, name_cap - 1);
                    name_out[name_cap - 1] = '\0';
                }
                break;
            }
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
    return result;
}

int py_resolve(py_api_t *api) {
    memset(api, 0, sizeof(*api));
    char base[64];
    HMODULE h = find_python_module(base, sizeof(base));
    if (!h) return RP_ERR_NOTSUP;
    api->py_base = (void*)h;

    /* 版本号提取：python39.dll -> 3.9 ; python3.9.dll -> 3.9 */
    if (strncmp(base, "python", 6) == 0) {
        const char *p = base + 6;
        if (*p == '3' && p[1] == '.') {
            snprintf(api->version, sizeof(api->version), "%s", p);
        } else if (p[0] == '3' && p[1] >= '0' && p[1] <= '9') {
            snprintf(api->version, sizeof(api->version), "3.%c", p[1]);
        }
    }

    /* 解析导出符号（CPython C-API，ABI 稳定） */
#define RESOLVE(field, name) do { \
        api->field = (void*)GetProcAddress(h, name); \
        if (!api->field) { return RP_ERR_NOTSUP; } \
    } while (0)

    RESOLVE(PyRun_SimpleString, "PyRun_SimpleString");
    RESOLVE(PyGILState_Ensure,   "PyGILState_Ensure");
    RESOLVE(PyGILState_Release,  "PyGILState_Release");
    RESOLVE(PyImport_AddModule,  "PyImport_AddModule");
    RESOLVE(PyImport_GetModuleDict, "PyImport_GetModuleDict");
    RESOLVE(Py_IsInitialized,    "Py_IsInitialized");
#undef RESOLVE
    return RP_OK;
}

int py_run_code(py_api_t *api, const char *code) {
    if (api->Py_IsInitialized && !api->Py_IsInitialized()) {
        return RP_ERR_NOTSUP;   /* 游戏尚未初始化 Python，无法注入 */
    }
    void *state = api->PyGILState_Ensure();
    int rc = api->PyRun_SimpleString(code);
    api->PyGILState_Release(state);
    return rc == 0 ? RP_OK : RP_ERR_GENERIC;
}

#else /* 非 Windows：占位实现 */
int py_resolve(py_api_t *api) { (void)api; return RP_ERR_NOTSUP; }
int py_run_code(py_api_t *api, const char *code) { (void)api; (void)code; return RP_ERR_NOTSUP; }
#endif
