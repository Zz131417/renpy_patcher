/* tests/test_inject_probe.c（仅 Windows）
 * 集成测试：验证「DLL 热注入」真的生效。
 *   默认：把探针 DLL 注入【当前进程】（自注入），走 rp_inject_dll 的完整链路
 *         （OpenProcess→VirtualAllocEx→WriteProcessMemory→CreateRemoteThread→LoadLibrary），
 *         轮询 Temp 下的标记文件确认探针 DLL 的 DllMain 被真正执行。
 *   可选：argv[2] 指定一个外部可执行文件时，改为启动该进程并注入（用于本机手动测跨进程注入，
 *         例如 test_inject_probe <probe.dll> notepad.exe）。
 * 不依赖 notepad.exe，因此在 CI runner 上也能稳定通过。
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

    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    DWORD myPid = GetCurrentProcessId();
    char marker[1024];
    snprintf(marker, sizeof(marker), "%srp_inject_probe_%lu.txt", tmp, (unsigned long)myPid);

    int ok = 0;
    if (argc > 2 && argv[2][0]) {
        /* 跨进程模式（手动用）：启动外部进程并注入 */
        const char *exe = argv[2];
        STARTUPINFOA si; PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));
        if (!CreateProcessA(exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            printf("[FAIL] 无法启动目标进程: %s (err=%lu)\n", exe, GetLastError());
            return 1;
        }
        DWORD pid = pi.dwProcessId;
        printf("[OK]   已启动目标进程 %s (pid=%lu)\n", exe, (unsigned long)pid);
        int rc = rp_inject_dll((int)pid, probe);
        if (rc != RP_OK) {
            printf("[FAIL] rp_inject_dll 返回 %d（注入失败）\n", rc);
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            DeleteFileA(marker);
            return 1;
        }
        printf("[OK]   rp_inject_dll 调用成功\n");
        for (int i = 0; i < 50; i++) { Sleep(100); if (file_exists(marker)) { ok = 1; break; } }
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    } else {
        /* 默认自注入模式：验证注入机制本身（DllMain 被执行） */
        printf("[OK]   自注入当前进程 (pid=%lu)\n", (unsigned long)myPid);
        int rc = rp_inject_dll((int)myPid, probe);
        if (rc != RP_OK) {
            printf("[FAIL] rp_inject_dll 返回 %d（注入失败）\n", rc);
            DeleteFileA(marker);
            return 1;
        }
        printf("[OK]   rp_inject_dll 调用成功\n");
        for (int i = 0; i < 50; i++) { Sleep(100); if (file_exists(marker)) { ok = 1; break; } }
    }

    DeleteFileA(marker);

    if (ok) {
        printf("\nDLL 热注入集成测试 PASSED ✅\n");
        return 0;
    }
    printf("\n[FAIL] 注入后未检测到 DLL 生效标记（DllMain 未执行？）\n");
    return 1;
}
