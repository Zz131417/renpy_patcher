// gui/Program.cpp —— C++/CLI 应用入口（仅 Windows / .NET Framework）
#include <windows.h>
#include "MainForm.h"

using namespace System;
using namespace System::Windows::Forms;

[STAThread]
int main(array<String^>^ args)
{
    (void)args;
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    Application::Run(gcnew RenPyPatcherGUI::MainForm());
    return 0;
}
