// gui/MainForm.cpp —— WinForms 主窗体实现（C++/CLI）
#include "MainForm.h"

using namespace System;
using namespace System::Windows::Forms;
using namespace System::IO;
using namespace RenPyPatcherGUI;

namespace RenPyPatcherGUI {

static std::string ToUtf8(String^ s) {
    return msclr::interop::marshal_as<std::string>(s);
}

/* 解析分卷尺寸：100M / 1G / 500K / 纯数字。失败或空返回 0。 */
static size_t ParseSize(String^ s) {
    if (String::IsNullOrWhiteSpace(s)) return 0;
    std::string str = ToUtf8(s->Trim());
    const char *p = str.c_str();
    char *end = nullptr;
    double v = strtod(p, &end);
    if (end == p) return 0;
    unsigned long long mult = 1;
    if (*end) {
        switch (tolower((unsigned char)*end)) {
            case 'k': mult = 1024ULL; break;
            case 'm': mult = 1024ULL * 1024; break;
            case 'g': mult = 1024ULL * 1024 * 1024; break;
            case 't': mult = 1024ULL * 1024 * 1024 * 1024; break;
            default:  mult = 1; break;
        }
    }
    return (size_t)(v * (double)mult);
}

/* C++/CLI 托管类里不能捕获托管句柄，也不能有静态局部托管变量。
 * 用 gcroot 持有 ListBox^，供 game_list 的静态原生回调使用。 */
static msclr::gcroot<ListBox^> g_gamesList;

static int __cdecl GameListCallback(const game_entry_t *g, void *ud) {
    (void)ud;
    ListBox^ lb = g_gamesList;
    if (lb != nullptr && g != nullptr) {
        lb->Items->Add(gcnew String(g->name) + L"  ->  " + gcnew String(g->path));
    }
    return 0;
}

MainForm::MainForm()
{
    this->Text = gcnew String("Ren'Py Patcher GUI ") + gcnew String(RPATCHER_VERSION_STRING);
    this->ClientSize = Drawing::Size(760, 720);
    this->StartPosition = FormStartPosition::CenterScreen;

    log_init("patcher.log");
    log_set_tag("gui");
    game_mgr_init(nullptr);

    int y = 12;

    // ============ 游戏管理 ============
    GroupBox^ gbGame = gcnew GroupBox();
    gbGame->Text = L"游戏管理 (别名登记表)";
    gbGame->Location = Drawing::Point(12, y);
    gbGame->Size = Drawing::Size(736, 100);
    txtAlias = gcnew TextBox();       txtAlias->Location = Drawing::Point(12, 22);   txtAlias->Size = Drawing::Size(110, 23);
    txtGamePath = gcnew TextBox();    txtGamePath->Location = Drawing::Point(130, 22); txtGamePath->Size = Drawing::Size(360, 23);
    btnBrowseGame = gcnew Button();   btnBrowseGame->Location = Drawing::Point(495, 20); btnBrowseGame->Size = Drawing::Size(60, 25); btnBrowseGame->Text = L"浏览";
    btnAddGame = gcnew Button();      btnAddGame->Location = Drawing::Point(560, 20); btnAddGame->Size = Drawing::Size(80, 25); btnAddGame->Text = L"添加";
    btnRemoveGame = gcnew Button();   btnRemoveGame->Location = Drawing::Point(645, 20); btnRemoveGame->Size = Drawing::Size(80, 25); btnRemoveGame->Text = L"删除";
    lstGames = gcnew ListBox();       lstGames->Location = Drawing::Point(12, 52); lstGames->Size = Drawing::Size(610, 42);
    btnRefreshGames = gcnew Button(); btnRefreshGames->Location = Drawing::Point(630, 52); btnRefreshGames->Size = Drawing::Size(95, 38); btnRefreshGames->Text = L"刷新列表";
    gbGame->Controls->AddRange(gcnew array<Control^>{ txtAlias, txtGamePath, btnBrowseGame, btnAddGame, btnRemoveGame, lstGames, btnRefreshGames });
    this->Controls->Add(gbGame);

    btnBrowseGame->Click += gcnew EventHandler(this, &MainForm::btnBrowseGame_Click);
    btnAddGame->Click    += gcnew EventHandler(this, &MainForm::btnAddGame_Click);
    btnRemoveGame->Click += gcnew EventHandler(this, &MainForm::btnRemoveGame_Click);
    btnRefreshGames->Click += gcnew EventHandler(this, &MainForm::btnRefreshGames_Click);

    y += 112;

    // ============ 解包 ============
    GroupBox^ gbU = gcnew GroupBox();
    gbU->Text = L"解包 (Unpack)";
    gbU->Location = Drawing::Point(12, y); gbU->Size = Drawing::Size(736, 64);
    txtUnpackIn = gcnew TextBox();    txtUnpackIn->Location = Drawing::Point(12, 22); txtUnpackIn->Size = Drawing::Size(300, 23);
    btnBrowseUnpackIn = gcnew Button(); btnBrowseUnpackIn->Location = Drawing::Point(317, 20); btnBrowseUnpackIn->Size = Drawing::Size(60, 25); btnBrowseUnpackIn->Text = L"浏览";
    txtUnpackOut = gcnew TextBox();   txtUnpackOut->Location = Drawing::Point(382, 22); txtUnpackOut->Size = Drawing::Size(220, 23);
    btnBrowseUnpackOut = gcnew Button(); btnBrowseUnpackOut->Location = Drawing::Point(607, 20); btnBrowseUnpackOut->Size = Drawing::Size(60, 25); btnBrowseUnpackOut->Text = L"目录";
    btnUnpack = gcnew Button();       btnUnpack->Location = Drawing::Point(670, 20); btnUnpack->Size = Drawing::Size(58, 25); btnUnpack->Text = L"执行";
    gbU->Controls->AddRange(gcnew array<Control^>{ txtUnpackIn, btnBrowseUnpackIn, txtUnpackOut, btnBrowseUnpackOut, btnUnpack });
    this->Controls->Add(gbU);
    btnBrowseUnpackIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseUnpackIn_Click);
    btnBrowseUnpackOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseUnpackOut_Click);
    btnUnpack->Click += gcnew EventHandler(this, &MainForm::btnUnpack_Click);

    y += 76;

    // ============ 注入（build） ============
    GroupBox^ gbB = gcnew GroupBox();
    gbB->Text = L"注入 (Build：解包→合并mod→重打包，自动写来源声明)";
    gbB->Location = Drawing::Point(12, y); gbB->Size = Drawing::Size(736, 90);
    txtBuildIn = gcnew TextBox();    txtBuildIn->Location = Drawing::Point(12, 20); txtBuildIn->Size = Drawing::Size(300, 23);
    btnBrowseBuildIn = gcnew Button(); btnBrowseBuildIn->Location = Drawing::Point(317, 18); btnBrowseBuildIn->Size = Drawing::Size(60, 25); btnBrowseBuildIn->Text = L"浏览";
    txtModDir = gcnew TextBox();     txtModDir->Location = Drawing::Point(382, 20); txtModDir->Size = Drawing::Size(180, 23);
    btnBrowseMod = gcnew Button();   btnBrowseMod->Location = Drawing::Point(567, 18); btnBrowseMod->Size = Drawing::Size(60, 25); btnBrowseMod->Text = L"目录";
    txtBuildOut = gcnew TextBox();   txtBuildOut->Location = Drawing::Point(12, 50); txtBuildOut->Size = Drawing::Size(300, 23);
    btnBrowseBuildOut = gcnew Button(); btnBrowseBuildOut->Location = Drawing::Point(317, 48); btnBrowseBuildOut->Size = Drawing::Size(60, 25); btnBrowseBuildOut->Text = L"浏览";
    btnBuild = gcnew Button();       btnBuild->Location = Drawing::Point(382, 48); btnBuild->Size = Drawing::Size(120, 25); btnBuild->Text = L"执行注入";
    gbB->Controls->AddRange(gcnew array<Control^>{ txtBuildIn, btnBrowseBuildIn, txtModDir, btnBrowseMod, txtBuildOut, btnBrowseBuildOut, btnBuild });
    this->Controls->Add(gbB);
    btnBrowseBuildIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseBuildIn_Click);
    btnBrowseMod->Click += gcnew EventHandler(this, &MainForm::btnBrowseMod_Click);
    btnBrowseBuildOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseBuildOut_Click);
    btnBuild->Click += gcnew EventHandler(this, &MainForm::btnBuild_Click);

    y += 102;

    // ============ 来源声明 ============
    GroupBox^ gbC = gcnew GroupBox();
    gbC->Text = L"来源声明 (Credit：给已有包标注开源项目注入/汉化)";
    gbC->Location = Drawing::Point(12, y); gbC->Size = Drawing::Size(736, 64);
    txtCreditIn = gcnew TextBox();   txtCreditIn->Location = Drawing::Point(12, 22); txtCreditIn->Size = Drawing::Size(300, 23);
    btnBrowseCreditIn = gcnew Button(); btnBrowseCreditIn->Location = Drawing::Point(317, 20); btnBrowseCreditIn->Size = Drawing::Size(60, 25); btnBrowseCreditIn->Text = L"浏览";
    txtCreditOut = gcnew TextBox();  txtCreditOut->Location = Drawing::Point(382, 22); txtCreditOut->Size = Drawing::Size(220, 23);
    btnBrowseCreditOut = gcnew Button(); btnBrowseCreditOut->Location = Drawing::Point(607, 20); btnBrowseCreditOut->Size = Drawing::Size(60, 25); btnBrowseCreditOut->Text = L"浏览";
    btnCredit = gcnew Button();      btnCredit->Location = Drawing::Point(670, 20); btnCredit->Size = Drawing::Size(58, 25); btnCredit->Text = L"执行";
    gbC->Controls->AddRange(gcnew array<Control^>{ txtCreditIn, btnBrowseCreditIn, txtCreditOut, btnBrowseCreditOut, btnCredit });
    this->Controls->Add(gbC);
    btnBrowseCreditIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseCreditIn_Click);
    btnBrowseCreditOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseCreditOut_Click);
    btnCredit->Click += gcnew EventHandler(this, &MainForm::btnCredit_Click);

    y += 76;

    // ============ 导出 ============
    GroupBox^ gbE = gcnew GroupBox();
    gbE->Text = L"导出 (Export：目录打包成标准 zip)";
    gbE->Location = Drawing::Point(12, y); gbE->Size = Drawing::Size(736, 64);
    txtExportDir = gcnew TextBox();  txtExportDir->Location = Drawing::Point(12, 22); txtExportDir->Size = Drawing::Size(260, 23);
    btnBrowseExportDir = gcnew Button(); btnBrowseExportDir->Location = Drawing::Point(277, 20); btnBrowseExportDir->Size = Drawing::Size(60, 25); btnBrowseExportDir->Text = L"目录";
    txtExportOut = gcnew TextBox();  txtExportOut->Location = Drawing::Point(342, 22); txtExportOut->Size = Drawing::Size(220, 23);
    btnBrowseExportOut = gcnew Button(); btnBrowseExportOut->Location = Drawing::Point(567, 20); btnBrowseExportOut->Size = Drawing::Size(60, 25); btnBrowseExportOut->Text = L"浏览";
    txtSplit = gcnew TextBox();      txtSplit->Location = Drawing::Point(632, 22); txtSplit->Size = Drawing::Size(60, 23);
    btnExport = gcnew Button();      btnExport->Location = Drawing::Point(695, 20); btnExport->Size = Drawing::Size(55, 25); btnExport->Text = L"导出";
    gbE->Controls->AddRange(gcnew array<Control^>{ txtExportDir, btnBrowseExportDir, txtExportOut, btnBrowseExportOut, txtSplit, btnExport });
    this->Controls->Add(gbE);
    btnBrowseExportDir->Click += gcnew EventHandler(this, &MainForm::btnBrowseExportDir_Click);
    btnBrowseExportOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseExportOut_Click);
    btnExport->Click += gcnew EventHandler(this, &MainForm::btnExport_Click);

    y += 76;

    // ============ 日志 ============
    GroupBox^ gbLog = gcnew GroupBox();
    gbLog->Text = L"日志 (详细见 patcher.log)";
    gbLog->Location = Drawing::Point(12, y); gbLog->Size = Drawing::Size(736, 120);
    txtLog = gcnew TextBox();
    txtLog->Location = Drawing::Point(12, 20); txtLog->Size = Drawing::Size(640, 86);
    txtLog->Multiline = true; txtLog->ScrollBars = ScrollBars::Vertical; txtLog->ReadOnly = true;
    btnClearLog = gcnew Button(); btnClearLog->Location = Drawing::Point(660, 50); btnClearLog->Size = Drawing::Size(65, 28); btnClearLog->Text = L"清空";
    gbLog->Controls->AddRange(gcnew array<Control^>{ txtLog, btnClearLog });
    this->Controls->Add(gbLog);
    btnClearLog->Click += gcnew EventHandler(this, &MainForm::btnClearLog_Click);

    RefreshGames();
    AppendLog(gcnew String("Ren'Py Patcher GUI ") + gcnew String(RPATCHER_VERSION_STRING) + L" 已启动");
}

void MainForm::AppendLog(String^ msg)
{
    txtLog->AppendText(msg + L"\r\n");
    txtLog->ScrollToCaret();
}

void MainForm::BrowseFile(TextBox^ tb, String^ filter)
{
    OpenFileDialog^ d = gcnew OpenFileDialog();
    d->Filter = filter;
    d->FileName = tb->Text;
    if (d->ShowDialog() == System::Windows::Forms::DialogResult::OK) tb->Text = d->FileName;
}

void MainForm::BrowseFolder(TextBox^ tb)
{
    FolderBrowserDialog^ d = gcnew FolderBrowserDialog();
    if (d->ShowDialog() == System::Windows::Forms::DialogResult::OK) tb->Text = d->SelectedPath;
}

void MainForm::RefreshGames()
{
    lstGames->Items->Clear();
    g_gamesList = lstGames;            /* 由静态原生回调读取（托管类内不能捕获托管句柄） */
    game_list(GameListCallback, nullptr);
    g_gamesList = nullptr;
}

void MainForm::btnAddGame_Click(Object^, EventArgs^)
{
    std::string name = ToUtf8(txtAlias->Text->Trim());
    std::string path = ToUtf8(txtGamePath->Text->Trim());
    if (name.empty() || path.empty()) { AppendLog(L"[游戏管理] 别名与路径均不能为空"); return; }
    int rc = game_add(name.c_str(), path.c_str(), "");
    if (rc == RP_OK) { AppendLog(gcnew String("已添加游戏: ") + txtAlias->Text->Trim()); RefreshGames(); }
    else AppendLog(gcnew String("[游戏管理] 添加失败，错误码 ") + rc.ToString());
}

void MainForm::btnRemoveGame_Click(Object^, EventArgs^)
{
    if (lstGames->SelectedIndex < 0) { AppendLog(L"[游戏管理] 请先在列表选择要删除的游戏"); return; }
    String^ item = safe_cast<String^>(lstGames->SelectedItem);
    int sp = item->IndexOf(L"  ->  ");
    String^ name = (sp > 0) ? item->Substring(0, sp) : item;
    int rc = game_remove(ToUtf8(name).c_str());
    if (rc == RP_OK) { AppendLog(gcnew String("已删除游戏: ") + name); RefreshGames(); }
    else AppendLog(gcnew String("[游戏管理] 删除失败，错误码 ") + rc.ToString());
}

void MainForm::btnRefreshGames_Click(Object^, EventArgs^)
{
    RefreshGames();
    AppendLog(L"[游戏管理] 已刷新");
}

void MainForm::btnUnpack_Click(Object^, EventArgs^)
{
    std::string in  = ToUtf8(txtUnpackIn->Text->Trim());
    std::string out = ToUtf8(txtUnpackOut->Text->Trim());
    if (in.empty()) { AppendLog(L"[解包] 请输入 .rpa 或别名"); return; }
    if (out.empty()) out = "out";
    AppendLog(gcnew String("[解包] 开始: ") + txtUnpackIn->Text->Trim());
    int rc = rpa_v3_unpack(in.c_str(), out.c_str());
    AppendLog(rc == RP_OK ? L"[解包] 完成 ✅" : gcnew String("[解包] 失败，错误码 ") + rc.ToString());
}

void MainForm::btnBuild_Click(Object^, EventArgs^)
{
    std::string game = ToUtf8(txtBuildIn->Text->Trim());
    std::string mod  = ToUtf8(txtModDir->Text->Trim());
    std::string out  = ToUtf8(txtBuildOut->Text->Trim());
    if (game.empty() || mod.empty()) { AppendLog(L"[注入] 游戏与 mod 目录不能为空"); return; }
    if (out.empty()) out = "patched.rpa";
    AppendLog(gcnew String("[注入] 开始: ") + txtBuildIn->Text->Trim() + L" + " + txtModDir->Text->Trim());
    int rc = direct_inject_build(game.c_str(), mod.c_str(), out.c_str());
    AppendLog(rc == RP_OK ? L"[注入] 完成 ✅ (已自动写入来源声明)" : gcnew String("[注入] 失败，错误码 ") + rc.ToString());
}

void MainForm::btnCredit_Click(Object^, EventArgs^)
{
    std::string in  = ToUtf8(txtCreditIn->Text->Trim());
    std::string out = ToUtf8(txtCreditOut->Text->Trim());
    if (in.empty()) { AppendLog(L"[声明] 请输入 .rpa 或别名"); return; }
    if (out.empty()) out = "credited.rpa";
    AppendLog(gcnew String("[声明] 开始: ") + txtCreditIn->Text->Trim());
    int rc = credit_add_to_rpa(in.c_str(), out.c_str(), in.c_str());
    AppendLog(rc == RP_OK ? L"[声明] 完成 ✅" : gcnew String("[声明] 失败，错误码 ") + rc.ToString());
}

void MainForm::btnExport_Click(Object^, EventArgs^)
{
    std::string dir = ToUtf8(txtExportDir->Text->Trim());
    std::string out = ToUtf8(txtExportOut->Text->Trim());
    if (dir.empty()) { AppendLog(L"[导出] 请选择要打包的目录"); return; }
    if (out.empty()) out = "patch.zip";
    size_t split = ParseSize(txtSplit->Text);
    AppendLog(gcnew String("[导出] 开始: ") + txtExportDir->Text->Trim());
    int rc = export_dir(dir.c_str(), out.c_str(), split, 1);
    AppendLog(rc == RP_OK ? L"[导出] 完成 ✅" : gcnew String("[导出] 失败，错误码 ") + rc.ToString());
}

void MainForm::btnClearLog_Click(Object^, EventArgs^)
{
    txtLog->Clear();
}

// 浏览按钮
void MainForm::btnBrowseGame_Click(Object^, EventArgs^)
{
    OpenFileDialog^ d = gcnew OpenFileDialog();
    d->Filter = L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*";
    if (d->ShowDialog() == System::Windows::Forms::DialogResult::OK) txtGamePath->Text = d->FileName;
}
void MainForm::btnBrowseUnpackIn_Click(Object^, EventArgs^)
{
    BrowseFile(txtUnpackIn, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseUnpackOut_Click(Object^, EventArgs^) { BrowseFolder(txtUnpackOut); }
void MainForm::btnBrowseBuildIn_Click(Object^, EventArgs^)
{
    BrowseFile(txtBuildIn, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseMod_Click(Object^, EventArgs^) { BrowseFolder(txtModDir); }
void MainForm::btnBrowseBuildOut_Click(Object^, EventArgs^)
{
    BrowseFile(txtBuildOut, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseCreditIn_Click(Object^, EventArgs^)
{
    BrowseFile(txtCreditIn, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseCreditOut_Click(Object^, EventArgs^)
{
    BrowseFile(txtCreditOut, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseExportDir_Click(Object^, EventArgs^) { BrowseFolder(txtExportDir); }
void MainForm::btnBrowseExportOut_Click(Object^, EventArgs^)
{
    BrowseFile(txtExportOut, L"ZIP 压缩包 (*.zip)|*.zip|所有文件 (*.*)|*.*");
}

} // namespace RenPyPatcherGUI
