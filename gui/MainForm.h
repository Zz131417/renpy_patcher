#pragma once

// C++/CLI (.NET) WinForms GUI for Ren'Py Patcher
// 仅 Windows 平台；底层复用同一套核心 C 库（核心 .c 直接编入本工程）。
//
// 布局为模块化：左侧导航栏 + 右侧页面容器，三个页面：
//   游戏管理 / 首页 / 设置。各页内容独立，互不干扰，便于后续扩展。

#include <msclr/marshal_cppstd.h>
#include <msclr/gcroot.h>

#include "core/version.h"
#include "rpa/rpa_v3.h"
#include "core/game_mgr.h"
#include "util/credit.h"
#include "util/log.h"
#include "inject/direct/direct_inject.h"
#include "inject/export/export_module.h"
#include "translate/rpy_dialog.h"
#include "translate/ai_translate.h"
#include "translate/json_io.h"
#include "util/zip_extract.h"
#include "util/sfx.h"
#include "util/resolve_source.h"
#include "analyze/branch.h"
#include "apk/apk_tool.h"

namespace RenPyPatcherGUI {

using namespace System;
using namespace System::Windows::Forms;
using namespace System::IO;
using namespace System::Drawing;
using namespace msclr::interop;
using namespace System::ComponentModel;

/* 页面索引 */
static const int PAGE_GAME     = 0;   // 游戏管理
static const int PAGE_HOME     = 1;   // 首页
static const int PAGE_SETTINGS = 2;   // 设置

/* 后台任务类型（用于检查更新之外的文件操作，避免界面卡死） */
public enum class OpType {
    Unpack, Build, Credit, Export, TransExtract, TransAI, TransBuild, ExportSfx, ExportLauncher, Branch, ExportApk
};

/// <summary>检查更新结果（后台线程 -> UI 线程传递用）</summary>
public ref class UpdateResult {
public:
    bool   HasUpdate;
    bool   Manual;
    String^ Version;
    String^ Tag;
    String^ Url;
    int    LatestBuild;
    int    CurBuild;
    bool   Prerelease;   // 远端版本是否为测试版（prerelease）
    String^ Error;
    UpdateResult(bool h, bool m, String^ v, String^ t, String^ u, int lb, int cb, bool pre, String^ err)
        : HasUpdate(h), Manual(m), Version(v), Tag(t), Url(u), LatestBuild(lb), CurBuild(cb), Prerelease(pre), Error(err) {}
};

public ref class MainForm : public Form
{
public:
    MainForm();

private:
    // ---- 导航栏 ----
    Panel^   navBar;
    Panel^   navHead;
    Button^  btnNavGame;
    Button^  btnNavHome;
    Button^  btnNavSettings;
    Label^   lblNavVer;

    // ---- 三个页面容器 ----
    Panel^   pnlGame;      // 游戏管理（所有实际操作）
    Panel^   pnlHome;      // 首页（概览 / 使用指引）
    Panel^   pnlSettings;  // 设置（AI 配置 / 关于）

    // ---- 游戏管理页：游戏管理 ----
    TextBox^  txtAlias;
    TextBox^  txtGamePath;
    Button^   btnBrowseGame;
    Button^   btnAddGame;
    Button^   btnRemoveGame;
    Button^   btnRefreshGames;
    ListBox^  lstGames;

    // ---- 游戏管理页：解包 ----
    TextBox^  txtUnpackIn;
    Button^   btnBrowseUnpackIn;
    TextBox^  txtUnpackOut;
    Button^   btnBrowseUnpackOut;
    Button^   btnUnpack;

    // ---- 游戏管理页：注入（build） ----
    TextBox^  txtBuildIn;
    Button^   btnBrowseBuildIn;
    TextBox^  txtModDir;
    Button^   btnBrowseMod;
    TextBox^  txtBuildOut;
    Button^   btnBrowseBuildOut;
    Button^   btnBuild;

    // ---- 游戏管理页：来源声明 ----
    TextBox^  txtCreditIn;
    Button^   btnBrowseCreditIn;
    TextBox^  txtCreditOut;
    Button^   btnBrowseCreditOut;
    Button^   btnCredit;

    // ---- 游戏管理页：导出 ----
    TextBox^  txtExportDir;
    Button^   btnBrowseExportDir;
    TextBox^  txtExportOut;
    Button^   btnBrowseExportOut;
    TextBox^  txtSplit;
    Button^   btnExport;
    TextBox^  txtExeRun;
    Button^   btnExportSfx;
    Button^   btnExportLauncher;
    // ---- 游戏管理页：导出 APK ----
    TextBox^  txtApkIn;
    Button^   btnBrowseApkIn;
    TextBox^  txtApkMod;
    Button^   btnBrowseApkMod;
    TextBox^  txtApkOut;
    Button^   btnBrowseApkOut;
    Button^   btnApkGen;

    // ---- 游戏管理页：聊天分支导图 ----
    TextBox^  txtBranchDir;
    Button^   btnBrowseBranchDir;
    Button^   btnBrowseBranchOut;
    ComboBox^ cmbBranchFmt;
    Button^   btnBranchGen;
    Button^   btnBranchSave;
    TextBox^  txtBranchOut;

    // ---- 游戏管理页：翻译流水线 ----
    TextBox^  txtTransDir;
    Button^   btnBrowseTransDir;
    TextBox^  txtTransCsv;
    Button^   btnBrowseTransCsv;
    TextBox^  txtTransLang;
    Button^   btnTransExtract;
    Button^   btnTransAI;
    Button^   btnTransBuild;

    // ---- 游戏管理页：日志 ----
    TextBox^  txtLog;
    Button^   btnClearLog;

    // ---- 设置页：AI 配置 ----
    TextBox^  txtAiBase;
    TextBox^  txtAiKey;
    TextBox^  txtAiModel;

    // ---- 首页 / 设置 的展示控件 ----
    TextBox^  txtHomeGuide;
    Label^    lblAbout;
    LinkLabel^ lnkProject;
    // ---- 检查更新 ----
    BackgroundWorker^ bgUpdate;
    Button^   btnCheckUpdate;
    Label^    lblUpdateStatus;
    CheckBox^ chkUpdateBeta;   // 接收测试版更新开关

    // ---- 底部状态栏 ----
    StatusStrip^        statusBar;
    ToolStripStatusLabel^ statusLabel;
    ToolStripStatusLabel^ statusVer;

    // ---- 文件操作后台任务 ----
    BackgroundWorker^ bgTask;
    Button^   _activeBtn;
    String^   _branchOutPath;   // 分支导图临时输出路径（DoWork 写入，Completed 读取显示）

    // ---- 页面构建 / 切换 ----
    void BuildNav();
    void BuildHome();
    void BuildGame();
    void BuildSettings();
    void ShowPage(int idx);

    // ---- 辅助 ----
    void AppendLog(String^ msg);
    String^ ResolveGameRpa(String^ token);   // .zip 整包 -> 解压定位内部 .rpa
    String^ FindStub();                       // 查找同目录 sfx_stub.exe
    String^ TempPath();                       // 临时目录（避开 Win32 宏 GetTempPath）
    void RefreshGames();
    void BrowseFile(TextBox^ tb, String^ filter);
    void BrowseFolder(TextBox^ tb);
    void SetStatus(String^ msg, bool busy);
    void RunOp(OpType type, Button^ btn, String^ opName, String^ a, String^ b, String^ c, String^ d);
    void RunOp(OpType type, Button^ btn, String^ opName, String^ a, String^ b, String^ c, String^ d, String^ base, String^ key, String^ model);

    // ---- 导航事件 ----
    void btnNavGame_Click(Object^, EventArgs^);
    void btnNavHome_Click(Object^, EventArgs^);
    void btnNavSettings_Click(Object^, EventArgs^);

    // ---- 游戏管理事件 ----
    void btnAddGame_Click(Object^, EventArgs^);
    void btnRemoveGame_Click(Object^, EventArgs^);
    void btnRefreshGames_Click(Object^, EventArgs^);
    void btnUnpack_Click(Object^, EventArgs^);
    void btnBuild_Click(Object^, EventArgs^);
    void btnCredit_Click(Object^, EventArgs^);
    void btnExport_Click(Object^, EventArgs^);
    void btnExportSfx_Click(Object^, EventArgs^);
    void btnExportLauncher_Click(Object^, EventArgs^);
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
    void btnBrowseApkIn_Click(Object^, EventArgs^);
    void btnBrowseApkMod_Click(Object^, EventArgs^);
    void btnBrowseApkOut_Click(Object^, EventArgs^);
    void btnApkGen_Click(Object^, EventArgs^);
    void btnBrowseTransDir_Click(Object^, EventArgs^);
    void btnBrowseTransCsv_Click(Object^, EventArgs^);
    void btnTransExtract_Click(Object^, EventArgs^);
    void btnTransAI_Click(Object^, EventArgs^);
    void btnTransBuild_Click(Object^, EventArgs^);
    void btnBrowseBranchDir_Click(Object^, EventArgs^);
    void btnBrowseBranchOut_Click(Object^, EventArgs^);
    void btnBranchGen_Click(Object^, EventArgs^);
    void btnBranchSave_Click(Object^, EventArgs^);

    // ---- 检查更新 ----
    void btnCheckUpdate_Click(Object^, EventArgs^);
    void chkUpdateBeta_CheckedChanged(Object^, EventArgs^);
    void bgUpdate_DoWork(Object^, DoWorkEventArgs^);
    void bgUpdate_RunWorkerCompleted(Object^, RunWorkerCompletedEventArgs^);

    // ---- 文件操作后台任务 ----
    void bgTask_DoWork(Object^, DoWorkEventArgs^);
    void bgTask_RunWorkerCompleted(Object^, RunWorkerCompletedEventArgs^);

    // ---- 设置持久化 ----
    String^ ConfigPath();
    void LoadConfig();
    void SaveConfig();
    void lnkProject_Click(Object^, LinkLabelLinkClickedEventArgs^);

    // ---- 报错显示 / 防崩溃 辅助 ----
    void SyncLog();
    void ReportFail(String^ op, int rc);
    void ShowTextDialog(String^ title, String^ content);   // 弹窗展示导图/长文本
    bool CheckFile(TextBox^ tb, String^ label);
    bool CheckDir(TextBox^ tb, String^ label);
};

} // namespace RenPyPatcherGUI
