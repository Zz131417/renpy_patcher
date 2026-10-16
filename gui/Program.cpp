// gui/Program.cpp —— C++/CLI 应用入口（仅 Windows / .NET Framework）
#include <windows.h>
#include "MainForm.h"

using namespace System;
using namespace System::Windows::Forms;

[STAThread]
int main(array<String^>^ args)
{
    (void)args;
    // 释放启动时自动分配的控制台窗口：GUI 不需要命令行，去掉黑框（先隐藏再释放，避免闪现）
    HWND hCon = GetConsoleWindow();
    if (hCon) ShowWindow(hCon, SW_HIDE);
    FreeConsole();

    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    Application::Run(gcnew RenPyPatcherGUI::MainForm());
    return 0;
}
