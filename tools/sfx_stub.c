/* tools/sfx_stub.c —— Ren'Py Patcher 自解压桩（仅 Windows 原生 C）
 *
 * 由 util/sfx.c 拼接在「目录 zip」之后生成自解压 exe：
 *   [ 本桩 exe ][ payload.zip ][ "RPEX" ][ stub_size(uint64 LE) ]
 *
 * 运行时：
 *   1. 读自身整个文件；
 *   2. 从尾部找 "RPEX" 哨兵，得到 stub_size（zip 起始偏移）；
 *   3. 把内置 zip 写到临时文件，调用 zip_extract 解压到「同目录/同名(去.exe)」文件夹；
 *   4. 若解压结果里有 RP_AUTORUN.txt，读取首行作为命令，在解压目录下自动运行（启动器模式）；
 *      否则弹出「解压完成」并打开该目录（纯安装包模式）。
 *
 * 依赖：renpy_core（zip_extract）、kernel32/shell32/user32。仅 Windows 编译。
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/zip_extract.h"

static void fatal(const char *msg)
{
    MessageBoxA(NULL, msg, "Ren'Py Patcher 自解压", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int nShow)
{
    (void)hInst; (void)hPrev; (void)cmdline; (void)nShow;

    char self[MAX_PATH];
    if (!GetModuleFileNameA(NULL, self, MAX_PATH)) fatal("无法获取自身路径");

    HANDLE fh = CreateFileA(self, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE) fatal("无法读取自身文件");
    LARGE_INTEGER fsz;
    if (!GetFileSizeEx(fh, &fsz)) { CloseHandle(fh); fatal("无法获取文件大小"); }
    unsigned char *buf = (unsigned char *)malloc((size_t)fsz.QuadPart + 1);
    if (!buf) { CloseHandle(fh); fatal("内存不足"); }
    DWORD rd = 0;
    if (!ReadFile(fh, buf, (DWORD)fsz.QuadPart, &rd, NULL)) { CloseHandle(fh); free(buf); fatal("读取自身失败"); }
    CloseHandle(fh);

    /* 从尾部回扫 "RPEX" 哨兵 */
    unsigned long long stub_size = 0;
    int ok = 0;
    if (fsz.QuadPart >= 12) {
        size_t i = (size_t)fsz.QuadPart - 12;
        while (1) {
            if (memcmp(buf + i, "RPEX", 4) == 0) {
                stub_size = 0;
                for (int k = 0; k < 8; k++)
                    stub_size |= ((unsigned long long)buf[i + 4 + k]) << (8 * k);
                ok = 1; break;
            }
            if (i == 0) break;
            i--;
        }
    }
    if (!ok) { free(buf); fatal("自解压包结构损坏（找不到 RPEX 哨兵）"); }
    if (stub_size + 12 > (size_t)fsz.QuadPart) { free(buf); fatal("自解压包结构损坏（stub_size 越界）"); }

    size_t zip_off = (size_t)stub_size;
    size_t zip_len = (size_t)fsz.QuadPart - 12 - zip_off;

    /* 把内置 zip 写到临时文件 */
    char tmppath[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tmppath)) { free(buf); fatal("无法获取临时目录"); }
    char zippath[MAX_PATH];
    snprintf(zippath, MAX_PATH, "%srenpy_patcher_sfx_%u.zip", tmppath, GetCurrentProcessId());
    HANDLE zf = CreateFileA(zippath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (zf == INVALID_HANDLE_VALUE) { free(buf); fatal("无法创建临时 zip"); }
    DWORD w = 0;
    WriteFile(zf, buf + zip_off, (DWORD)zip_len, &w, NULL);
    CloseHandle(zf);
    free(buf);

    /* 解压目录：与自身同目录、同名（去掉 .exe） */
    char drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH], ext[MAX_PATH];
    _splitpath(self, drive, dir, fname, ext);
    char outdir[MAX_PATH];
    snprintf(outdir, MAX_PATH, "%s%s%s", drive, dir, fname);

    int n = 0;
    int rc = zip_extract(zippath, outdir, &n);
    DeleteFileA(zippath);
    if (rc != 0) {
        char m[256];
        snprintf(m, sizeof(m), "解压失败（错误码 %d）", rc);
        fatal(m);
    }

    /* 启动器模式？ */
    char auto_path[MAX_PATH];
    snprintf(auto_path, MAX_PATH, "%s\\RP_AUTORUN.txt", outdir);
    FILE *af = fopen(auto_path, "r");
    if (af) {
        char cmdline_buf[1024];
        if (fgets(cmdline_buf, sizeof(cmdline_buf), af)) {
            char *p = cmdline_buf;
            while (*p) { if (*p == '\r' || *p == '\n') { *p = '\0'; break; } p++; }
            fclose(af);
            STARTUPINFOA si; PROCESS_INFORMATION pi;
            memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
            if (CreateProcessA(NULL, cmdline_buf, NULL, NULL, FALSE, 0, NULL, outdir, &si, &pi)) {
                CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
                return 0;
            }
            char m[512];
            snprintf(m, sizeof(m), "解压完成，但启动命令执行失败：\n%s", cmdline_buf);
            MessageBoxA(NULL, m, "Ren'Py Patcher 自解压", MB_OK);
            return 0;
        }
        fclose(af);
    }

    /* 纯安装包模式 */
    char msg[600];
    snprintf(msg, sizeof(msg), "解压完成（%d 个文件）\n位置：%s", n, outdir);
    MessageBoxA(NULL, msg, "Ren'Py Patcher 自解压", MB_OK);
    ShellExecuteA(NULL, "open", outdir, NULL, NULL, SW_SHOWNORMAL);
    return 0;
}
