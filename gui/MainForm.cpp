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

MainForm::MainForm()
{
    this->Text = gcnew String("Ren'Py Patcher GUI ") + gcnew String(RPATCHER_VERSION_STRING);
    this->ClientSize = System::Drawing::Size(760, 720);
    this->StartPosition = FormStartPosition::CenterScreen;

    log_init("patcher.log");
    log_set_tag("gui");
    game_mgr_init(nullptr);

    int y = 12;

    // ============ 游戏管理 ============
    GroupBox^ gbGame = gcnew GroupBox();
    gbGame->Text = L"游戏管理 (别名登记表)";
    gbGame->Location = System::Drawing::Point(12, y);
    gbGame->Size = System::Drawing::Size(736, 100);
    txtAlias = gcnew TextBox();       txtAlias->Location = Point(12, 22);   txtAlias->Size = Size(110, 23);  txtAlias->PlaceholderText = L"别名";
    txtGamePath = gcnew TextBox();    txtGamePath->Location = Point(130, 22); txtGamePath->Size = Size(360, 23); txtGamePath->PlaceholderText = L"游戏 .rpa 路径";
    btnBrowseGame = gcnew Button();   btnBrowseGame->Location = Point(495, 20); btnBrowseGame->Size = Size(60, 25); btnBrowseGame->Text = L"浏览";
    btnAddGame = gcnew Button();      btnAddGame->Location = Point(560, 20); btnAddGame->Size = Size(80, 25); btnAddGame->Text = L"添加";
    btnRemoveGame = gcnew Button();   btnRemoveGame->Location = Point(645, 20); btnRemoveGame->Size = Size(80, 25); btnRemoveGame->Text = L"删除";
    lstGames = gcnew ListBox();       lstGames->Location = Point(12, 52); lstGames->Size = Size(610, 42);
    btnRefreshGames = gcnew Button(); btnRefreshGames->Location = Point(630, 52); btnRefreshGames->Size = Size(95, 38); btnRefreshGames->Text = L"刷新列表";
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
    gbU->Location = Point(12, y); gbU->Size = Size(736, 64);
    txtUnpackIn = gcnew TextBox();    txtUnpackIn->Location = Point(12, 22); txtUnpackIn->Size = Size(300, 23); txtUnpackIn->PlaceholderText = L"输入 .rpa (或别名)";
    btnBrowseUnpackIn = gcnew Button(); btnBrowseUnpackIn->Location = Point(317, 20); btnBrowseUnpackIn->Size = Size(60, 25); btnBrowseUnpackIn->Text = L"浏览";
    txtUnpackOut = gcnew TextBox();   txtUnpackOut->Location = Point(382, 22); txtUnpackOut->Size = Size(220, 23); txtUnpackOut->PlaceholderText = L"输出目录";
    btnBrowseUnpackOut = gcnew Button(); btnBrowseUnpackOut->Location = Point(607, 20); btnBrowseUnpackOut->Size = Size(60, 25); btnBrowseUnpackOut->Text = L"目录";
    btnUnpack = gcnew Button();       btnUnpack->Location = Point(670, 20); btnUnpack->Size = Size(58, 25); btnUnpack->Text = L"执行";
    gbU->Controls->AddRange(gcnew array<Control^>{ txtUnpackIn, btnBrowseUnpackIn, txtUnpackOut, btnBrowseUnpackOut, btnUnpack });
    this->Controls->Add(gbU);
    btnBrowseUnpackIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseUnpackIn_Click);
    btnBrowseUnpackOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseUnpackOut_Click);
    btnUnpack->Click += gcnew EventHandler(this, &MainForm::btnUnpack_Click);

    y += 76;

    // ============ 注入（build） ============
    GroupBox^ gbB = gcnew GroupBox();
    gbB->Text = L"注入 (Build：解包→合并mod→重打包，自动写来源声明)";
    gbB->Location = Point(12, y); gbB->Size = Size(736, 90);
    txtBuildIn = gcnew TextBox();    txtBuildIn->Location = Point(12, 20); txtBuildIn->Size = Size(300, 23); txtBuildIn->PlaceholderText = L"游戏 .rpa (或别名)";
    btnBrowseBuildIn = gcnew Button(); btnBrowseBuildIn->Location = Point(317, 18); btnBrowseBuildIn->Size = Size(60, 25); btnBrowseBuildIn->Text = L"浏览";
    txtModDir = gcnew TextBox();     txtModDir->Location = Point(382, 20); txtModDir->Size = Size(180, 23); txtModDir->PlaceholderText = L"mod 目录";
    btnBrowseMod = gcnew Button();   btnBrowseMod->Location = Point(567, 18); btnBrowseMod->Size = Size(60, 25); btnBrowseMod->Text = L"目录";
    txtBuildOut = gcnew TextBox();   txtBuildOut->Location = Point(12, 50); txtBuildOut->Size = Size(300, 23); txtBuildOut->PlaceholderText = L"输出 .rpa";
    btnBrowseBuildOut = gcnew Button(); btnBrowseBuildOut->Location = Point(317, 48); btnBrowseBuildOut->Size = Size(60, 25); btnBrowseBuildOut->Text = L"浏览";
    btnBuild = gcnew Button();       btnBuild->Location = Point(382, 48); btnBuild->Size = Size(120, 25); btnBuild->Text = L"执行注入";
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
    gbC->Location = Point(12, y); gbC->Size = Size(736, 64);
    txtCreditIn = gcnew TextBox();   txtCreditIn->Location = Point(12, 22); txtCreditIn->Size = Size(300, 23); txtCreditIn->PlaceholderText = L"输入 .rpa (或别名)";
    btnBrowseCreditIn = gcnew Button(); btnBrowseCreditIn->Location = Point(317, 20); btnBrowseCreditIn->Size = Size(60, 25); btnBrowseCreditIn->Text = L"浏览";
    txtCreditOut = gcnew TextBox();  txtCreditOut->Location = Point(382, 22); txtCreditOut->Size = Size(220, 23); txtCreditOut->PlaceholderText = L"输出 .rpa";
    btnBrowseCreditOut = gcnew Button(); btnBrowseCreditOut->Location = Point(607, 20); btnBrowseCreditOut->Size = Size(60, 25); btnBrowseCreditOut->Text = L"浏览";
    btnCredit = gcnew Button();      btnCredit->Location = Point(670, 20); btnCredit->Size = Size(58, 25); btnCredit->Text = L"执行";
    gbC->Controls->AddRange(gcnew array<Control^>{ txtCreditIn, btnBrowseCreditIn, txtCreditOut, btnBrowseCreditOut, btnCredit });
    this->Controls->Add(gbC);
    btnBrowseCreditIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseCreditIn_Click);
    btnBrowseCreditOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseCreditOut_Click);
    btnCredit->Click += gcnew EventHandler(this, &MainForm::btnCredit_Click);

    y += 76;

    // ============ 导出 ============
    GroupBox^ gbE = gcnew GroupBox();
    gbE->Text = L"导出 (Export：目录打包成标准 zip)";
    gbE->Location = Point(12, y); gbE->Size = Size(736, 64);
    txtExportDir = gcnew TextBox();  txtExportDir->Location = Point(12, 22); txtExportDir->Size = Size(260, 23); txtExportDir->PlaceholderText = L"要打包的目录";
    btnBrowseExportDir = gcnew Button(); btnBrowseExportDir->Location = Point(277, 20); btnBrowseExportDir->Size = Size(60, 25); btnBrowseExportDir->Text = L"目录";
    txtExportOut = gcnew TextBox();  txtExportOut->Location = Point(342, 22); txtExportOut->Size = Size(220, 23); txtExportOut->PlaceholderText = L"输出 .zip";
    btnBrowseExportOut = gcnew Button(); btnBrowseExportOut->Location = Point(567, 20); btnBrowseExportOut->Size = Size(60, 25); btnBrowseExportOut->Text = L"浏览";
    txtSplit = gcnew TextBox();      txtSplit->Location = Point(632, 22); txtSplit->Size = Size(60, 23); txtSplit->PlaceholderText = L"分卷如100M";
    btnExport = gcnew Button();      btnExport->Location = Point(695, 20); btnExport->Size = Size(55, 25); btnExport->Text = L"导出";
    gbE->Controls->AddRange(gcnew array<Control^>{ txtExportDir, btnBrowseExportDir, txtExportOut, btnBrowseExportOut, txtSplit, btnExport });
    this->Controls->Add(gbE);
    btnBrowseExportDir->Click += gcnew EventHandler(this, &MainForm::btnBrowseExportDir_Click);
    btnBrowseExportOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseExportOut_Click);
    btnExport->Click += gcnew EventHandler(this, &MainForm::btnExport_Click);

    y += 76;

    // ============ 日志 ============
    GroupBox^ gbLog = gcnew GroupBox();
    gbLog->Text = L"日志 (详细见 patcher.log)";
    gbLog->Location = Point(12, y); gbLog->Size = Size(736, 120);
    txtLog = gcnew TextBox();
    txtLog->Location = Point(12, 20); txtLog->Size = Size(640, 86);
    txtLog->Multiline = true; txtLog->ScrollBars = ScrollBars::Vertical; txtLog->ReadOnly = true;
    btnClearLog = gcnew Button(); btnClearLog->Location = Point(660, 50); btnClearLog->Size = Size(65, 28); btnClearLog->Text = L"清空";
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
    if (d->ShowDialog() == DialogResult::OK) tb->Text = d->FileName;
}

void MainForm::BrowseFolder(TextBox^ tb)
{
    FolderBrowserDialog^ d = gcnew FolderBrowserDialog();
    if (d->ShowDialog() == DialogResult::OK) tb->Text = d->SelectedPath;
}

void MainForm::RefreshGames()
{
    lstGames->Items->Clear();
    /* C 回调无法直接持有托管句柄，用静态句柄中转 */
    static ListBox^ s_lb = nullptr;
    s_lb = lstGames;
    game_list([](const game_entry_t *g, void *ud) -> int {
        (void)ud;
        s_lb->Items->Add(gcnew String(g->name) + L"  ->  " + gcnew String(g->path));
        return 0;
    }, nullptr);
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

// 浏览按钮（独立事件）
void MainForm::btnBrowseGame_Click(Object^, EventArgs^)
{
    OpenFileDialog^ d = gcnew OpenFileDialog();
    d->Filter = L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*";
    if (d->ShowDialog() == DialogResult::OK) txtGamePath->Text = d->FileName;
}
void MainForm::btnBrowseUnpackIn_Click(Object^, EventArgs^)
{
    BrowseFile(txtUnpackIn, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseUnpackOut_Click(Object^, EventArgs^) { BrowseFolder(txtUnpackOut); }
void MainForm::btnBrowseBuildIn_Click(Object^)
{
    BrowseFile(txtBuildIn, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseMod_Click(Object^, EventArgs^) { BrowseFolder(txtModDir); }
void MainForm::btnBrowseBuildOut_Click(Object^)
{
    BrowseFile(txtBuildOut, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseCreditIn_Click(Object^)
{
    BrowseFile(txtCreditIn, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseCreditOut_Click(Object^)
{
    BrowseFile(txtCreditOut, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
}
void MainForm::btnBrowseExportDir_Click(Object^, EventArgs^) { BrowseFolder(txtExportDir); }
void MainForm::btnBrowseExportOut_Click(Object^)
{
    BrowseFile(txtExportOut, L"ZIP 压缩包 (*.zip)|*.zip|所有文件 (*.*)|*.*");
}

} // namespace RenPyPatcherGUI
