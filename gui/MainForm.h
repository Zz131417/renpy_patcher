#pragma once

// C++/CLI (.NET) WinForms GUI for Ren'Py Patcher
// 仅 Windows 平台；底层复用同一套核心 C 库（核心 .c 直接编入本工程）。

#include <msclr/marshal_cppstd.h>
#include <msclr/gcroot.h>

#include "core/version.h"
#include "rpa/rpa_v3.h"
#include "core/game_mgr.h"
#include "util/credit.h"
#include "util/log.h"
#include "inject/direct/direct_inject.h"
#include "inject/export/export_module.h"

namespace RenPyPatcherGUI {

using namespace System;
using namespace System::Windows::Forms;
using namespace System::IO;
using namespace System::Drawing;
using namespace msclr::interop;

/// <summary>
/// 主窗体：提供游戏管理、解包、注入、来源声明、导出等 GUI 操作。
/// 功能与 CLI 一一对应，共用核心 C 实现。
/// </summary>
public ref class MainForm : public Form
{
public:
    MainForm();

private:
    // ---- 游戏管理 ----
    TextBox^  txtAlias;
    TextBox^  txtGamePath;
    Button^   btnBrowseGame;
    Button^   btnAddGame;
    Button^   btnRemoveGame;
    Button^   btnRefreshGames;
    ListBox^  lstGames;

    // ---- 解包 ----
    TextBox^  txtUnpackIn;
    Button^   btnBrowseUnpackIn;
    TextBox^  txtUnpackOut;
    Button^   btnBrowseUnpackOut;
    Button^   btnUnpack;

    // ---- 注入（build） ----
    TextBox^  txtBuildIn;
    Button^   btnBrowseBuildIn;
    TextBox^  txtModDir;
    Button^   btnBrowseMod;
    TextBox^  txtBuildOut;
    Button^   btnBrowseBuildOut;
    Button^   btnBuild;

    // ---- 来源声明 ----
    TextBox^  txtCreditIn;
    Button^   btnBrowseCreditIn;
    TextBox^  txtCreditOut;
    Button^   btnBrowseCreditOut;
    Button^   btnCredit;

    // ---- 导出 ----
    TextBox^  txtExportDir;
    Button^   btnBrowseExportDir;
    TextBox^  txtExportOut;
    Button^   btnBrowseExportOut;
    TextBox^  txtSplit;
    Button^   btnExport;

    // ---- 日志 ----
    TextBox^  txtLog;
    Button^   btnClearLog;

    // ---- 辅助 ----
    void AppendLog(String^ msg);
    void RefreshGames();
    void BrowseFile(TextBox^ tb, String^ filter);
    void BrowseFolder(TextBox^ tb);

    // ---- 事件 ----
    void btnAddGame_Click(Object^, EventArgs^);
    void btnRemoveGame_Click(Object^, EventArgs^);
    void btnRefreshGames_Click(Object^, EventArgs^);
    void btnUnpack_Click(Object^, EventArgs^);
    void btnBuild_Click(Object^, EventArgs^);
    void btnCredit_Click(Object^, EventArgs^);
    void btnExport_Click(Object^, EventArgs^);
    void btnClearLog_Click(Object^, EventArgs^);
    void btnBrowseGame_Click(Object^, EventArgs^);
    void btnBrowseUnpackIn_Click(Object^, EventArgs^);
    void btnBrowseUnpackOut_Click(Object^, EventArgs^);
    void btnBrowseBuildIn_Click(Object^, EventArgs^);
    void btnBrowseMod_Click(Object^, EventArgs^);
    void btnBrowseBuildOut_Click(Object^, EventArgs^);
    void btnBrowseCreditIn_Click(Object^, EventArgs^);
    void btnBrowseCreditOut_Click(Object^, EventArgs^);
    void btnBrowseExportDir_Click(Object^, EventArgs^);
    void btnBrowseExportOut_Click(Object^, EventArgs^);

    // ---- 报错显示 / 防崩溃 辅助 ----
    void SyncLog();
    void ReportFail(String^ op, int rc);
    bool CheckFile(String^ tb, String^ label);
    bool CheckDir(String^ tb, String^ label);
};

} // namespace RenPyPatcherGUI
