// gui/Program.cpp —— C++/CLI 应用入口（仅 Windows / .NET Framework）
#include <windows.h>
#include "MainForm.h"

using namespace System;
using namespace System::Windows::Forms;

// 使用窗口子系统，避免启动时的控制台黑框（影响美观）
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

[STAThread]
int main(array<String^>^ args)
{
    (void)args;
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    Application::Run(gcnew RenPyPatcherGUI::MainForm());
    return 0;
}
