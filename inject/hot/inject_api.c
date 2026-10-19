/* inject/hot/inject_api.c —— 热注入入口（注入器逻辑）
 *
 * Windows：用远程线程将 agent.dll 注入目标进程（LoadLibrary 技术），
 *          并通过命名内存映射把 mod 路径交给 agent。
 * 非 Windows：返回 RP_ERR_NOTSUP（沙箱/平台不支持）。
 */
#include "agent.h"
#include "inject/hot/inject_dll.h"
#include "core/plugin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

static const char *agent_dll_path(void) {
    static char path[MAX_PATH];
    if (path[0]) return path;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char *p = strrchr(path, '\\');
    if (p) *(p + 1) = '\0';
    strcat(path, "agent.dll");
    return path;
}

int hot_inject(int pid, const char *mod_path) {
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                     PAGE_READWRITE, 0, MAX_PATH, RP_HOT_MAP_NAME);
    if (!hMap) { fprintf(stderr, "创建共享内存失败\n"); return RP_ERR_IO; }
    void *view = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, 0);
    if (!view) { CloseHandle(hMap); return RP_ERR_IO; }
    strncpy((char*)view, mod_path, MAX_PATH - 1);
    ((char*)view)[MAX_PATH - 1] = '\0';
    FlushViewOfFile(view, strlen(mod_path) + 1);
    UnmapViewOfFile(view);   /* 映射对象本身仍存活（句柄未关） */

    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, (DWORD)pid);
    if (!hProc) {
        fprintf(stderr, "无法打开进程 pid=%d（权限不足？）\n", pid);
        CloseHandle(hMap);
        return RP_ERR_IO;
    }

    const char *dll = agent_dll_path();
    size_t dll_len = strlen(dll) + 1;
    LPVOID remote = VirtualAllocEx(hProc, NULL, dll_len, MEM_COMMIT, PAGE_READWRITE);
    if (!remote) { CloseHandle(hProc); CloseHandle(hMap); return RP_ERR_NOMEM; }
    SIZE_T written = 0;
    if (!WriteProcessMemory(hProc, remote, dll, dll_len, &written)) {
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc); CloseHandle(hMap);
        return RP_ERR_IO;
    }

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    LPTHREAD_START_ROUTINE load = (LPTHREAD_START_ROUTINE)
        GetProcAddress(hKernel, "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, load, remote, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc); CloseHandle(hMap);
        return RP_ERR_IO;
    }
    WaitForSingleObject(hThread, INFINITE);
    DWORD exit = 0;
    GetExitCodeThread(hThread, &exit);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);

    HANDLE hDone = OpenEventA(EVENT_ALL_ACCESS, FALSE, RP_HOT_DONE_EVENT);
    if (hDone) {
        WaitForSingleObject(hDone, 5000);
        CloseHandle(hDone);
    }

    CloseHandle(hProc);
    CloseHandle(hMap);

    if (exit == 0) {
        printf("hot-inject: agent.dll 已注入 pid=%d，mod=%s\n", pid, mod_path);
        return RP_OK;
    }
    fprintf(stderr, "hot-inject: LoadLibrary 返回 %lu（agent.dll 缺失或位数不匹配？）\n", exit);
    return RP_ERR_PLUGIN;
}

#else /* 非 Windows */
int hot_inject(int pid, const char *mod_path) {
    (void)pid; (void)mod_path;
    fprintf(stderr, "hot-inject: 仅支持 Windows 平台\n");
    return RP_ERR_NOTSUP;
}
#endif

/* 模块 run：hotinject --pid <pid> --mod <mod_dir> */
static int run(int argc, char **argv) {
    const char *pid_s = NULL, *mod = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--pid") && i + 1 < argc) pid_s = argv[++i];
        else if (!strcmp(argv[i], "--mod") && i + 1 < argc) mod = argv[++i];
    }
    if (!pid_s || !mod) { fprintf(stderr, "用法: hotinject --pid <pid> --mod <mod_dir>\n"); return 2; }
    int pid = atoi(pid_s);
    if (pid <= 0) { fprintf(stderr, "非法 pid\n"); return 2; }
    return hot_inject(pid, mod);
}

const inject_module_t g_inject_hot = {
    .id = "hot",
    .desc = "热注入：进程注入 agent.dll + 定位CPython + 运行时加载mod（Windows）",
    .run = run,
};
};

/* ===================== 通用 DLL 注入（任意 DLL，不依赖 agent/Python） ===================== */

#ifdef _WIN32

/* 把 dll_path 注入到已打开的进程 hProc（调用方负责开关句柄） */
static int inject_dll_into_handle(HANDLE hProc, const char *dll_path) {
    size_t len = strlen(dll_path) + 1;
    LPVOID remote = VirtualAllocEx(hProc, NULL, len, MEM_COMMIT, PAGE_READWRITE);
    if (!remote) return RP_ERR_NOMEM;
    SIZE_T written = 0;
    if (!WriteProcessMemory(hProc, remote, dll_path, len, &written)) {
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        return RP_ERR_IO;
    }
    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    LPTHREAD_START_ROUTINE load =
        (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel, "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, load, remote, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        return RP_ERR_IO;
    }
    WaitForSingleObject(hThread, INFINITE);
    DWORD exit = 0;
    GetExitCodeThread(hThread, &exit);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    if (exit == 0) return RP_OK;
    fprintf(stderr, "inject-dll: LoadLibrary 返回 %lu（DLL 缺失 / 位数不匹配 / 依赖不全？）\n", exit);
    return RP_ERR_PLUGIN;
}

int inject_dll_into_pid(int pid, const char *dll_path) {
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, (DWORD)pid);
    if (!hProc) {
        fprintf(stderr, "inject-dll: 无法打开进程 pid=%d（权限不足？）\n", pid);
        return RP_ERR_IO;
    }
    int rc = inject_dll_into_handle(hProc, dll_path);
    CloseHandle(hProc);
    if (rc == RP_OK) printf("inject-dll: 已注入 %s -> pid=%d\n", dll_path, pid);
    return rc;
}

int inject_dll_into_exe(const char *exe_path, const char *dll_path, int delay_ms) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(exe_path, NULL, NULL, NULL, FALSE,
                        delay_ms > 0 ? 0 : CREATE_SUSPENDED,
                        NULL, NULL, &si, &pi)) {
        fprintf(stderr, "inject-dll: 无法启动进程: %s\n", exe_path);
        return RP_ERR_IO;
    }
    int rc;
    if (delay_ms > 0) {
        Sleep((DWORD)delay_ms);
        rc = inject_dll_into_handle(pi.hProcess, dll_path);
    } else {
        rc = inject_dll_into_handle(pi.hProcess, dll_path);
        ResumeThread(pi.hThread);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (rc == RP_OK) printf("inject-dll: 已注入 %s -> %s\n", dll_path, exe_path);
    return rc;
}

#else /* 非 Windows */
int inject_dll_into_pid(int pid, const char *dll_path) {
    (void)pid; (void)dll_path;
    fprintf(stderr, "inject-dll: 仅支持 Windows 平台\n");
    return RP_ERR_NOTSUP;
}
int inject_dll_into_exe(const char *exe_path, const char *dll_path, int delay_ms) {
    (void)exe_path; (void)dll_path; (void)delay_ms;
    fprintf(stderr, "inject-dll: 仅支持 Windows 平台\n");
    return RP_ERR_NOTSUP;
}
#endif

