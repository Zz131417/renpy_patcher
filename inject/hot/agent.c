/* inject/hot/agent.c —— 被注入到游戏进程的 DLL 载荷
 *
 * 流程：DllMain 在 DLL_PROCESS_ATTACH 时开线程 →
 *   1. 等待注入器把 mod 路径写入命名内存映射
 *   2. 定位 CPython 并解析符号
 *   3. 持有 GIL，运行 bootstrap：把 mod 目录加入 sys.path 并 import mod_main
 *
 * 注意：本文件仅 Windows 编译（依赖 windows.h / CPython 注入）。
 */
#include "agent.h"
#include "py_bridge.h"
#include "core/plugin.h"

#ifdef _WIN32
#include <windows.h>
#include <string.h>
#include <stdio.h>

static DWORD WINAPI agent_thread(LPVOID lp) {
    (void)lp;

    /* 1. 读取 mod 路径（注入器先建好映射再注入本 DLL） */
    char mod_path[MAX_PATH] = {0};
    HANDLE hMap = OpenFileMappingA(FILE_MAP_READ, FALSE, RP_HOT_MAP_NAME);
    if (hMap) {
        void *view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        if (view) {
            strncpy(mod_path, (const char*)view, sizeof(mod_path) - 1);
            UnmapViewOfFile(view);
        }
        CloseHandle(hMap);
    }
    if (mod_path[0] == '\0') {
        OutputDebugStringA("[renpy_patcher] agent: 未收到 mod 路径\n");
        return 1;
    }

    /* 通知注入器：已读到配置，可关闭映射 */
    HANDLE hDone = OpenEventA(EVENT_ALL_ACCESS, FALSE, RP_HOT_DONE_EVENT);
    if (hDone) { SetEvent(hDone); CloseHandle(hDone); }

    /* 2. 定位 CPython */
    py_api_t api;
    if (py_resolve(&api) != RP_OK) {
        OutputDebugStringA("[renpy_patcher] agent: 未找到 python 模块\n");
        return 2;
    }

    /* 3. 构造 bootstrap 并运行 */
    char buf[1024];
    _snprintf(buf, sizeof(buf),
        "import sys\n"
        "_mp = r'%s'\n"
        "if _mp not in sys.path:\n"
        "    sys.path.insert(0, _mp)\n"
        "try:\n"
        "    import mod_main\n"
        "    if hasattr(mod_main, 'on_attach'):\n"
        "        mod_main.on_attach()\n"
        "except Exception as _e:\n"
        "    sys.stderr.write('hot mod load error: %r\\n' % _e)\n",
        mod_path);

    int rc = py_run_code(&api, buf);
    OutputDebugStringA(rc == RP_OK
        ? "[renpy_patcher] agent: mod 已加载\n"
        : "[renpy_patcher] agent: mod 加载失败\n");
    return rc == RP_OK ? 0 : 3;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    (void)hinst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        /* 不阻塞加载线程：另起线程做注入工作 */
        HANDLE h = CreateThread(NULL, 0, agent_thread, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    return TRUE;
}

#endif /* _WIN32 */
