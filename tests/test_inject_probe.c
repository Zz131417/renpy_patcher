/* tests/test_inject_probe.c（仅 Windows）
 * 集成测试：验证「DLL 热注入」真的生效。
 *   1) 启动一个目标进程（默认 notepad.exe，可 argv[2] 指定）
 *   2) 用 patcher_core.dll 的 rp_inject_dll 把探针 DLL（argv[1]）注入该进程
 *   3) 轮询 Temp 下的标记文件（rp_inject_probe_<pid>.txt）确认 DLL 已被加载执行
 *   4) 清理（结束目标进程、删除标记文件）
 * 这是对 rp_inject_dll 的端到端真注入验证（不是仅检查接口可达）。
 */
#include <stdio.h>
#include <windows.h>
#include "core_api/core_api.h"

/* core_api.h 未暴露状态码宏，本地定义测试所需（与 core/plugin.h 一致：RP_OK=0） */
#ifndef RP_OK
#define RP_OK 0
#endif

static int file_exists(const char *p) {
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int main(int argc, char **argv)
{
    rp_set_log_level(0);

    const char *probe = (argc > 1 && argv[1][0]) ? argv[1] : "inject_probe_dll.dll";
    const char *exe   = (argc > 2 && argv[2][0]) ? argv[2] : "notepad.exe";

    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("[FAIL] 无法启动目标进程: %s (err=%lu)\n", exe, GetLastError());
        return 1;
    }
    DWORD pid = pi.dwProcessId;
    printf("[OK]   已启动目标进程 %s (pid=%lu)\n", exe, (unsigned long)pid);

    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    char marker[1024];
    snprintf(marker, sizeof(marker), "%srp_inject_probe_%lu.txt", tmp, (unsigned long)pid);

    int rc = rp_inject_dll((int)pid, probe);
    if (rc != RP_OK) {
        printf("[FAIL] rp_inject_dll 返回 %d（注入失败）\n", rc);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return 1;
    }
    printf("[OK]   rp_inject_dll 调用成功\n");

    int ok = 0;
    for (int i = 0; i < 50; i++) {        /* 最多等 5 秒让 DllMain 跑完 */
        Sleep(100);
        if (file_exists(marker)) { ok = 1; break; }
    }

    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    DeleteFileA(marker);

    if (ok) {
        printf("\nDLL 热注入集成测试 PASSED ✅\n");
        return 0;
    }
    printf("\n[FAIL] 注入后未检测到 DLL 生效标记（DllMain 未执行？）\n");
    return 1;
}
