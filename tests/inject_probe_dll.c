/* tests/inject_probe_dll.c
 * 测试用的探针 DLL（仅 Windows）。被注入到目标进程后，DllMain 在 DLL_PROCESS_ATTACH
 * 时于系统 Temp 目录写一个带目标进程 PID 的标记文件，证明 DLL 真正被加载执行。
 * 集成测试 test_inject_probe 注入本 DLL 后检查该标记文件即可确认「热注入」成功。
 */
#include <windows.h>
#include <stdio.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        char tmp[MAX_PATH];
        if (GetTempPathA(MAX_PATH, tmp))
        {
            DWORD pid = GetCurrentProcessId();
            char path[1024];
            snprintf(path, sizeof(path), "%srp_inject_probe_%lu.txt", tmp, (unsigned long)pid);
            FILE *fp = fopen(path, "w");
            if (fp)
            {
                fprintf(fp, "injected pid=%lu ok\n", (unsigned long)pid);
                fclose(fp);
            }
        }
        (void)hModule; (void)lpReserved;
    }
    return TRUE;
}
