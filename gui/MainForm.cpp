// gui/MainForm.cpp —— WinForms 主窗体实现（C++/CLI）
// 模块化布局：左侧导航栏 + 右侧三页面（游戏管理 / 首页 / 设置）
#include "MainForm.h"
#include <stdlib.h>

static long g_logPos = 0;   // 已读取的 patcher.log 字节数，用于增量同步到日志框

using namespace System;
using namespace System::Windows::Forms;
using namespace System::IO;
using namespace RenPyPatcherGUI;
using namespace System::Net;
using namespace System::ComponentModel;

/* ===================== 主题（C++/CLI 不允许全局静态托管类型，统一收进静态类 UI） ===================== */
public ref class UI abstract sealed {
public:
    static initonly Drawing::Color ACCENT    = Drawing::Color::FromArgb(59, 130, 246);   // blue-500
    static initonly Drawing::Color ACCENT_D  = Drawing::Color::FromArgb(37, 99, 235);    // hover
    static initonly Drawing::Color SIDEBAR   = Drawing::Color::FromArgb(31, 41, 55);     // gray-800
    static initonly Drawing::Color SIDEHI    = Drawing::Color::FromArgb(55, 65, 81);     // hover
    static initonly Drawing::Color PANEL     = Drawing::Color::FromArgb(243, 244, 246);  // gray-100
    static initonly Drawing::Color TEXT      = Drawing::Color::FromArgb(17, 24, 39);
    static initonly Drawing::Color MUTE      = Drawing::Color::FromArgb(107, 114, 128);
    static initonly Drawing::Color CONSOLE_B = Drawing::Color::FromArgb(28, 30, 38);
    static initonly Drawing::Color CONSOLE_F = Drawing::Color::FromArgb(214, 222, 235);
    static initonly Drawing::Font^  FONT     = gcnew Drawing::Font(L"Microsoft YaHei", 9.5f);
    static initonly Drawing::Font^  FONT_B   = gcnew Drawing::Font(L"Microsoft YaHei", 9.5f, FontStyle::Bold);
    static initonly Drawing::Font^  FONT_TITLE = gcnew Drawing::Font(L"Microsoft YaHei", 20.0f, FontStyle::Bold);
    static initonly Drawing::Font^  FONT_SUB  = gcnew Drawing::Font(L"Microsoft YaHei", 10.0f);
    static initonly Drawing::Font^  FONT_MONO = gcnew Drawing::Font(L"Consolas", 9.5f);
};

/* 统一按钮样式：primary=强调色实心，否则浅灰 */
static void StyleBtn(Button^ b, bool primary)
{
    b->FlatStyle = FlatStyle::Flat;
    b->Font = UI::FONT;
    b->UseVisualStyleBackColor = false;
    b->Cursor = Cursors::Hand;
    b->FlatAppearance->BorderSize = 0;
    if (primary) {
        b->BackColor = UI::ACCENT;
        b->ForeColor = Drawing::Color::White;
        b->FlatAppearance->MouseOverBackColor = UI::ACCENT_D;
        b->FlatAppearance->MouseDownBackColor = UI::ACCENT_D;
    } else {
        b->BackColor = Drawing::Color::FromArgb(229, 231, 235);
        b->ForeColor = UI::TEXT;
        b->FlatAppearance->MouseOverBackColor = Drawing::Color::FromArgb(209, 213, 219);
        b->FlatAppearance->MouseDownBackColor = Drawing::Color::FromArgb(209, 213, 219);
    }
}

/* 统一分组框样式（标题用强调色） */
static GroupBox^ MakeGroup(String^ title, int x, int y, int w, int h)
{
    GroupBox^ g = gcnew GroupBox();
    g->Location = Drawing::Point(x, y); g->Size = Drawing::Size(w, h);
    g->Text = title;
    g->Font = UI::FONT_B;
    g->ForeColor = UI::ACCENT;
    g->UseCompatibleTextRendering = true;
    return g;
}

/* 统一标签 */
static Label^ MakeLabel(String^ text, int x, int y, int w, int h, bool mute)
{
    Label^ l = gcnew Label();
    l->Location = Drawing::Point(x, y); l->Size = Drawing::Size(w, h);
    l->Text = text; l->Font = UI::FONT;
    l->ForeColor = mute ? UI::MUTE : UI::TEXT;
    l->TextAlign = Drawing::ContentAlignment::MiddleLeft;
    return l;
}

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

/* 从简单 JSON 文本中提取字段（用于解析 version.json / 检查更新） */
static String^ JsonField(String^ json, String^ key)
{
    String^ pat = "\"" + key + "\":";
    int i = json->IndexOf(pat);
    if (i < 0) return nullptr;
    int c = i + pat->Length;
    while (c < json->Length && (json[c]==' '||json[c]=='\t')) c++;
    if (c >= json->Length) return nullptr;
    if (json[c] == '"') {
        int end = json->IndexOf('"', c+1);
        if (end < 0) return nullptr;
        return json->Substring(c+1, end - c - 1);
    }
    int end = c;
    while (end < json->Length && json[end]>='0' && json[end]<='9') end++;
    if (end == c) return nullptr;
    return json->Substring(c, end - c);
}
static int JsonInt(String^ json, String^ key)
{
    String^ v = JsonField(json, key);
    if (v == nullptr) return 0;
    try { return Int32::Parse(v); } catch (...) { return 0; }
}

/* 失败信息（不触碰 UI 控件，可在后台线程调用） */
static String^ FailMsg(String^ op, int rc)
{
    String^ meaning = gcnew String(rp_strerror(rc));
    String^ detail  = gcnew String(rp_last_error());
    String^ m = L"[" + op + L"] 失败：错误码 " + rc.ToString() + L" (" + meaning + L")";
    if (detail != nullptr && detail->Length > 0) m += L" | 详情: " + detail;
    return m;
}

/* ===================== 后台任务上下文 ===================== */
ref class TaskCtx {
public:
    OpType   Type;
    String^  a;
    String^  b;
    String^  c;
    String^  d;
    String^  opName;
    int      rc;
    bool     ok;
    String^  exMsg;
    String^  msg;
    String^  outPath;   // TransAI 完成后回填给 txtTransCsv
    String^  base;   // AI 配置
    String^  key;
    String^  model;
    Button^  btn;
};

/* ===================== 构造函数 ===================== */
MainForm::MainForm()
{
    this->Text = gcnew String("Ren'Py Patcher GUI ") + gcnew String(RPATCHER_VERSION_STRING);
    this->ClientSize = Drawing::Size(944, 772);
    this->MinimumSize = Drawing::Size(780, 520);
    this->StartPosition = FormStartPosition::CenterScreen;
    this->Font = UI::FONT;
    this->BackColor = Drawing::Color::White;
    this->DoubleBuffered = true;

    log_init("patcher.log");
    log_set_tag("gui");
    game_mgr_init(nullptr);

    BuildNav();
    BuildHome();
    BuildGame();
    BuildSettings();
    LoadConfig();   // 载入设置（如「接收测试版更新」开关），须在 bgUpdate 启动前

    // 底部状态栏
    statusBar = gcnew StatusStrip();
    statusBar->Dock = DockStyle::Bottom;
    statusBar->BackColor = UI::PANEL;
    statusLabel = gcnew ToolStripStatusLabel();
    statusLabel->Text = L"就绪";
    statusLabel->Spring = true;
    statusLabel->Font = UI::FONT;
    statusVer = gcnew ToolStripStatusLabel();
    statusVer->Text = gcnew String("v") + gcnew String(RPATCHER_VERSION_STRING);
    statusVer->BorderSides = ToolStripStatusLabelBorderSides::Left;
    statusVer->Font = UI::FONT;
    statusBar->Items->AddRange(gcnew array<ToolStripItem^>{ statusLabel, statusVer });

    this->Controls->Add(navBar);
    this->Controls->Add(pnlHome);
    this->Controls->Add(pnlGame);
    this->Controls->Add(pnlSettings);
    this->Controls->Add(statusBar);

    ShowPage(PAGE_GAME);   // 默认进入「游戏管理」

    RefreshGames();
    AppendLog(gcnew String("Ren'Py Patcher GUI ") + gcnew String(RPATCHER_VERSION_STRING) + L" 已启动");
    SyncLog();

    // 检查更新（后台线程，不阻塞 UI；启动后静默检查一次）
    bgUpdate = gcnew BackgroundWorker();
    bgUpdate->DoWork += gcnew DoWorkEventHandler(this, &MainForm::bgUpdate_DoWork);
    bgUpdate->RunWorkerCompleted += gcnew RunWorkerCompletedEventHandler(this, &MainForm::bgUpdate_RunWorkerCompleted);
    if (!bgUpdate->IsBusy) bgUpdate->RunWorkerAsync(false);

    // 文件操作后台任务
    bgTask = gcnew BackgroundWorker();
    bgTask->DoWork += gcnew DoWorkEventHandler(this, &MainForm::bgTask_DoWork);
    bgTask->RunWorkerCompleted += gcnew RunWorkerCompletedEventHandler(this, &MainForm::bgTask_RunWorkerCompleted);
}

/* ===================== 导航栏 ===================== */
void MainForm::BuildNav()
{
    navBar = gcnew Panel();
    navBar->Location = Drawing::Point(0, 0);
    navBar->Size = Drawing::Size(168, 772);
    navBar->BackColor = UI::SIDEBAR;

    navHead = gcnew Panel();
    navHead->Location = Drawing::Point(0, 0);
    navHead->Size = Drawing::Size(168, 84);
    navHead->BackColor = Drawing::Color::FromArgb(17, 24, 39);
    Label^ app = gcnew Label();
    app->Location = Drawing::Point(16, 18); app->Size = Drawing::Size(140, 26);
    app->Text = L"Ren'Py Patcher";
    app->Font = gcnew Drawing::Font(L"Microsoft YaHei", 14, FontStyle::Bold);
    app->ForeColor = Drawing::Color::White;
    Label^ sub = gcnew Label();
    sub->Location = Drawing::Point(16, 46); sub->Size = Drawing::Size(140, 20);
    sub->Text = L"解包 / 注入 / 汉化";
    sub->Font = UI::FONT_SUB;
    sub->ForeColor = Drawing::Color::FromArgb(148, 163, 184);
    navHead->Controls->AddRange(gcnew array<Control^>{ app, sub });

    btnNavGame = gcnew Button();
    btnNavGame->Location = Drawing::Point(14, 104);  btnNavGame->Size = Drawing::Size(140, 40);
    btnNavGame->Text = L"游戏管理";
    btnNavHome = gcnew Button();
    btnNavHome->Location = Drawing::Point(14, 152);  btnNavHome->Size = Drawing::Size(140, 40);
    btnNavHome->Text = L"首页";
    btnNavSettings = gcnew Button();
    btnNavSettings->Location = Drawing::Point(14, 200); btnNavSettings->Size = Drawing::Size(140, 40);
    btnNavSettings->Text = L"设置";

    for each (Button^ b in gcnew array<Button^>{ btnNavGame, btnNavHome, btnNavSettings }) {
        b->FlatStyle = FlatStyle::Flat;
        b->Font = UI::FONT_B;
        b->UseVisualStyleBackColor = false;
        b->Cursor = Cursors::Hand;
        b->ForeColor = Drawing::Color::FromArgb(203, 213, 225);
        b->BackColor = UI::SIDEBAR;
        b->FlatAppearance->BorderSize = 0;
        b->FlatAppearance->MouseOverBackColor = UI::SIDEHI;
        b->TextAlign = Drawing::ContentAlignment::MiddleLeft;
        b->Padding = System::Windows::Forms::Padding(14, 0, 0, 0);
    }

    lblNavVer = gcnew Label();
    lblNavVer->Location = Drawing::Point(14, 740); lblNavVer->Size = Drawing::Size(140, 24);
    lblNavVer->Text = gcnew String("v") + gcnew String(RPATCHER_VERSION_STRING);
    lblNavVer->Font = UI::FONT;
    lblNavVer->ForeColor = Drawing::Color::FromArgb(148, 163, 184);

    navBar->Controls->AddRange(gcnew array<Control^>{ navHead, btnNavGame, btnNavHome, btnNavSettings, lblNavVer });
    btnNavGame->Click    += gcnew EventHandler(this, &MainForm::btnNavGame_Click);
    btnNavHome->Click    += gcnew EventHandler(this, &MainForm::btnNavHome_Click);
    btnNavSettings->Click += gcnew EventHandler(this, &MainForm::btnNavSettings_Click);
}

void MainForm::ShowPage(int idx)
{
    pnlGame->Visible     = (idx == PAGE_GAME);
    pnlHome->Visible     = (idx == PAGE_HOME);
    pnlSettings->Visible = (idx == PAGE_SETTINGS);

    Drawing::Color sel = UI::ACCENT;
    Drawing::Color nor = UI::SIDEBAR;
    Drawing::Color selF = Drawing::Color::White;
    Drawing::Color norF = Drawing::Color::FromArgb(203, 213, 225);

    btnNavGame->BackColor    = (idx == PAGE_GAME)     ? sel : nor;
    btnNavGame->ForeColor    = (idx == PAGE_GAME)     ? selF : norF;
    btnNavHome->BackColor    = (idx == PAGE_HOME)     ? sel : nor;
    btnNavHome->ForeColor    = (idx == PAGE_HOME)     ? selF : norF;
    btnNavSettings->BackColor = (idx == PAGE_SETTINGS) ? sel : nor;
    btnNavSettings->ForeColor = (idx == PAGE_SETTINGS) ? selF : norF;
}

void MainForm::btnNavGame_Click(Object^, EventArgs^)    { ShowPage(PAGE_GAME); }
void MainForm::btnNavHome_Click(Object^, EventArgs^)    { ShowPage(PAGE_HOME); }
void MainForm::btnNavSettings_Click(Object^, EventArgs^) { ShowPage(PAGE_SETTINGS); }

/* ===================== 首页 ===================== */
void MainForm::BuildHome()
{
    pnlHome = gcnew Panel();
    pnlHome->Location = Drawing::Point(176, 8);
    pnlHome->Size = Drawing::Size(760, 724);
    pnlHome->AutoScroll = true;
    pnlHome->BackColor = Drawing::Color::White;

    Panel^ head = gcnew Panel();
    head->Location = Drawing::Point(0, 0);
    head->Size = Drawing::Size(760, 104);
    head->BackColor = UI::ACCENT;
    Label^ t = gcnew Label();
    t->Location = Drawing::Point(28, 20); t->Size = Drawing::Size(700, 40);
    t->Font = UI::FONT_TITLE; t->ForeColor = Drawing::Color::White;
    t->Text = L"Ren'Py Patcher";
    Label^ v = gcnew Label();
    v->Location = Drawing::Point(28, 64); v->Size = Drawing::Size(700, 26);
    v->Font = UI::FONT_SUB; v->ForeColor = Drawing::Color::FromArgb(219, 234, 254);
    v->Text = gcnew String("版本 ") + gcnew String(RPATCHER_VERSION_STRING)
              + L"   ·   解包 / 注入 / 汉化一站式工具";
    head->Controls->AddRange(gcnew array<Control^>{ t, v });

    txtHomeGuide = gcnew TextBox();
    txtHomeGuide->Location = Drawing::Point(28, 124);
    txtHomeGuide->Size = Drawing::Size(704, 420);
    txtHomeGuide->Multiline = true;
    txtHomeGuide->ScrollBars = ScrollBars::Vertical;
    txtHomeGuide->ReadOnly = true;
    txtHomeGuide->BorderStyle = BorderStyle::None;
    txtHomeGuide->BackColor = UI::PANEL;
    txtHomeGuide->Font = UI::FONT;
    txtHomeGuide->ForeColor = UI::TEXT;
    txtHomeGuide->Text = L"快速上手：\n\n"
        L"1. 游戏管理：登记游戏别名与资源包路径，方便后续用别名引用。\n"
        L"2. 解包（Unpack）：把游戏的 .rpa 资源包解出来，得到可改的目录。\n"
        L"3. 注入（Build）：把 mod 目录合并回资源包；会自动写入来源声明，\n"
        L"   方便合规地标注你基于哪些开源项目做了改动。\n"
        L"4. 翻译流水线：\n"
        L"     · 提取对话：把游戏脚本里的对话抽成 JSON；\n"
        L"     · AI 翻译：调用 OpenAI 兼容接口批量翻译（在「设置」里填 Key）；\n"
        L"     · 生成翻译 mod：把译文写成 Ren'Py 翻译 mod，再用「注入」打包进游戏。\n"
        L"5. 来源声明 / 导出：给现成资源包补来源声明；把目录打成标准 zip。\n\n"
        L"提示：所有操作日志都会实时显示在「游戏管理」页底部的日志框，\n"
        L"也可查看同目录下的 patcher.log。";

    Button^ b = gcnew Button();
    b->Location = Drawing::Point(28, 560); b->Size = Drawing::Size(170, 38);
    b->Text = L"开始使用 →";
    StyleBtn(b, true);
    b->Click += gcnew EventHandler(this, &MainForm::btnNavGame_Click);

    pnlHome->Controls->AddRange(gcnew array<Control^>{ head, txtHomeGuide, b });
}

/* ===================== 游戏管理（所有实际操作） ===================== */
void MainForm::BuildGame()
{
    pnlGame = gcnew Panel();
    pnlGame->Location = Drawing::Point(176, 8);
    pnlGame->Size = Drawing::Size(760, 724);
    pnlGame->AutoScroll = true;
    pnlGame->BackColor = Drawing::Color::White;

    int y = 12;

    // ============ 游戏管理 ============
    GroupBox^ gbGame = MakeGroup(L"游戏管理 (别名登记表)", 12, y, 736, 104);
    txtAlias = gcnew TextBox();       txtAlias->Location = Drawing::Point(14, 26);   txtAlias->Size = Drawing::Size(110, 23); txtAlias->Font = UI::FONT;
    txtGamePath = gcnew TextBox();    txtGamePath->Location = Drawing::Point(132, 26); txtGamePath->Size = Drawing::Size(346, 23); txtGamePath->Font = UI::FONT;
    btnBrowseGame = gcnew Button();   btnBrowseGame->Location = Drawing::Point(483, 24); btnBrowseGame->Size = Drawing::Size(60, 25); btnBrowseGame->Text = L"浏览";
    btnAddGame = gcnew Button();      btnAddGame->Location = Drawing::Point(548, 24); btnAddGame->Size = Drawing::Size(80, 25); btnAddGame->Text = L"添加";
    btnRemoveGame = gcnew Button();   btnRemoveGame->Location = Drawing::Point(633, 24); btnRemoveGame->Size = Drawing::Size(90, 25); btnRemoveGame->Text = L"删除";
    lstGames = gcnew ListBox();       lstGames->Location = Drawing::Point(14, 56); lstGames->Size = Drawing::Size(620, 42); lstGames->Font = UI::FONT;
    btnRefreshGames = gcnew Button(); btnRefreshGames->Location = Drawing::Point(638, 56); btnRefreshGames->Size = Drawing::Size(85, 38); btnRefreshGames->Text = L"刷新列表";
    gbGame->Controls->AddRange(gcnew array<Control^>{ txtAlias, txtGamePath, btnBrowseGame, btnAddGame, btnRemoveGame, lstGames, btnRefreshGames });
    pnlGame->Controls->Add(gbGame);
    StyleBtn(btnBrowseGame, false); StyleBtn(btnAddGame, true); StyleBtn(btnRemoveGame, false); StyleBtn(btnRefreshGames, false);
    btnBrowseGame->Click += gcnew EventHandler(this, &MainForm::btnBrowseGame_Click);
    btnAddGame->Click    += gcnew EventHandler(this, &MainForm::btnAddGame_Click);
    btnRemoveGame->Click += gcnew EventHandler(this, &MainForm::btnRemoveGame_Click);
    btnRefreshGames->Click += gcnew EventHandler(this, &MainForm::btnRefreshGames_Click);

    y += 116;

    // ============ 解包 ============
    GroupBox^ gbU = MakeGroup(L"解包 (Unpack)", 12, y, 736, 66);
    txtUnpackIn = gcnew TextBox();    txtUnpackIn->Location = Drawing::Point(14, 28); txtUnpackIn->Size = Drawing::Size(300, 23); txtUnpackIn->Font = UI::FONT;
    btnBrowseUnpackIn = gcnew Button(); btnBrowseUnpackIn->Location = Drawing::Point(319, 26); btnBrowseUnpackIn->Size = Drawing::Size(60, 25); btnBrowseUnpackIn->Text = L"浏览";
    txtUnpackOut = gcnew TextBox();   txtUnpackOut->Location = Drawing::Point(384, 28); txtUnpackOut->Size = Drawing::Size(212, 23); txtUnpackOut->Font = UI::FONT;
    btnBrowseUnpackOut = gcnew Button(); btnBrowseUnpackOut->Location = Drawing::Point(601, 26); btnBrowseUnpackOut->Size = Drawing::Size(60, 25); btnBrowseUnpackOut->Text = L"目录";
    btnUnpack = gcnew Button();       btnUnpack->Location = Drawing::Point(666, 26); btnUnpack->Size = Drawing::Size(60, 25); btnUnpack->Text = L"执行";
    gbU->Controls->AddRange(gcnew array<Control^>{ txtUnpackIn, btnBrowseUnpackIn, txtUnpackOut, btnBrowseUnpackOut, btnUnpack });
    pnlGame->Controls->Add(gbU);
    StyleBtn(btnBrowseUnpackIn, false); StyleBtn(btnBrowseUnpackOut, false); StyleBtn(btnUnpack, true);
    btnBrowseUnpackIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseUnpackIn_Click);
    btnBrowseUnpackOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseUnpackOut_Click);
    btnUnpack->Click += gcnew EventHandler(this, &MainForm::btnUnpack_Click);

    y += 78;

    // ============ 注入（build） ============
    GroupBox^ gbB = MakeGroup(L"注入 (Build：解包→合并mod→重打包，自动写来源声明)", 12, y, 736, 92);
    txtBuildIn = gcnew TextBox();    txtBuildIn->Location = Drawing::Point(14, 26); txtBuildIn->Size = Drawing::Size(300, 23); txtBuildIn->Font = UI::FONT;
    btnBrowseBuildIn = gcnew Button(); btnBrowseBuildIn->Location = Drawing::Point(319, 24); btnBrowseBuildIn->Size = Drawing::Size(60, 25); btnBrowseBuildIn->Text = L"浏览";
    txtModDir = gcnew TextBox();     txtModDir->Location = Drawing::Point(384, 26); txtModDir->Size = Drawing::Size(180, 23); txtModDir->Font = UI::FONT;
    btnBrowseMod = gcnew Button();   btnBrowseMod->Location = Drawing::Point(569, 24); btnBrowseMod->Size = Drawing::Size(60, 25); btnBrowseMod->Text = L"目录";
    txtBuildOut = gcnew TextBox();   txtBuildOut->Location = Drawing::Point(14, 56); txtBuildOut->Size = Drawing::Size(300, 23); txtBuildOut->Font = UI::FONT;
    btnBrowseBuildOut = gcnew Button(); btnBrowseBuildOut->Location = Drawing::Point(319, 54); btnBrowseBuildOut->Size = Drawing::Size(60, 25); btnBrowseBuildOut->Text = L"浏览";
    btnBuild = gcnew Button();       btnBuild->Location = Drawing::Point(384, 54); btnBuild->Size = Drawing::Size(120, 25); btnBuild->Text = L"执行注入";
    gbB->Controls->AddRange(gcnew array<Control^>{ txtBuildIn, btnBrowseBuildIn, txtModDir, btnBrowseMod, txtBuildOut, btnBrowseBuildOut, btnBuild });
    pnlGame->Controls->Add(gbB);
    StyleBtn(btnBrowseBuildIn, false); StyleBtn(btnBrowseMod, false); StyleBtn(btnBrowseBuildOut, false); StyleBtn(btnBuild, true);
    btnBrowseBuildIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseBuildIn_Click);
    btnBrowseMod->Click += gcnew EventHandler(this, &MainForm::btnBrowseMod_Click);
    btnBrowseBuildOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseBuildOut_Click);
    btnBuild->Click += gcnew EventHandler(this, &MainForm::btnBuild_Click);

    y += 104;

    // ============ 来源声明 ============
    GroupBox^ gbC = MakeGroup(L"来源声明 (Credit：给已有包标注开源项目注入/汉化)", 12, y, 736, 66);
    txtCreditIn = gcnew TextBox();   txtCreditIn->Location = Drawing::Point(14, 28); txtCreditIn->Size = Drawing::Size(300, 23); txtCreditIn->Font = UI::FONT;
    btnBrowseCreditIn = gcnew Button(); btnBrowseCreditIn->Location = Drawing::Point(319, 26); btnBrowseCreditIn->Size = Drawing::Size(60, 25); btnBrowseCreditIn->Text = L"浏览";
    txtCreditOut = gcnew TextBox();  txtCreditOut->Location = Drawing::Point(384, 28); txtCreditOut->Size = Drawing::Size(212, 23); txtCreditOut->Font = UI::FONT;
    btnBrowseCreditOut = gcnew Button(); btnBrowseCreditOut->Location = Drawing::Point(601, 26); btnBrowseCreditOut->Size = Drawing::Size(60, 25); btnBrowseCreditOut->Text = L"浏览";
    btnCredit = gcnew Button();      btnCredit->Location = Drawing::Point(666, 26); btnCredit->Size = Drawing::Size(60, 25); btnCredit->Text = L"执行";
    gbC->Controls->AddRange(gcnew array<Control^>{ txtCreditIn, btnBrowseCreditIn, txtCreditOut, btnBrowseCreditOut, btnCredit });
    pnlGame->Controls->Add(gbC);
    StyleBtn(btnBrowseCreditIn, false); StyleBtn(btnBrowseCreditOut, false); StyleBtn(btnCredit, true);
    btnBrowseCreditIn->Click += gcnew EventHandler(this, &MainForm::btnBrowseCreditIn_Click);
    btnBrowseCreditOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseCreditOut_Click);
    btnCredit->Click += gcnew EventHandler(this, &MainForm::btnCredit_Click);

    y += 78;

    // ============ 导出 ============
    GroupBox^ gbE = MakeGroup(L"导出 (Export：目录打包成标准 zip / 自解压 exe)", 12, y, 736, 112);
    txtExportDir = gcnew TextBox();  txtExportDir->Location = Drawing::Point(14, 28); txtExportDir->Size = Drawing::Size(252, 23); txtExportDir->Font = UI::FONT;
    btnBrowseExportDir = gcnew Button(); btnBrowseExportDir->Location = Drawing::Point(271, 26); btnBrowseExportDir->Size = Drawing::Size(60, 25); btnBrowseExportDir->Text = L"目录";
    txtExportOut = gcnew TextBox();  txtExportOut->Location = Drawing::Point(336, 28); txtExportOut->Size = Drawing::Size(212, 23); txtExportOut->Font = UI::FONT;
    btnBrowseExportOut = gcnew Button(); btnBrowseExportOut->Location = Drawing::Point(553, 26); btnBrowseExportOut->Size = Drawing::Size(60, 25); btnBrowseExportOut->Text = L"浏览";
    txtSplit = gcnew TextBox();      txtSplit->Location = Drawing::Point(618, 28); txtSplit->Size = Drawing::Size(54, 23); txtSplit->Font = UI::FONT; txtSplit->Text = L"0";
    btnExport = gcnew Button();      btnExport->Location = Drawing::Point(676, 26); btnExport->Size = Drawing::Size(56, 25); btnExport->Text = L"导出zip";
    Label^ lRun = MakeLabel(L"启动命令(启动器用)", 14, 60, 110, 23, true);
    txtExeRun = gcnew TextBox();     txtExeRun->Location = Drawing::Point(132, 58); txtExeRun->Size = Drawing::Size(300, 23); txtExeRun->Font = UI::FONT;
    txtExeRun->Text = L"game.exe";
    btnExportSfx = gcnew Button();     btnExportSfx->Location = Drawing::Point(444, 56); btnExportSfx->Size = Drawing::Size(120, 25); btnExportSfx->Text = L"导出自解压exe";
    btnExportLauncher = gcnew Button(); btnExportLauncher->Location = Drawing::Point(570, 56); btnExportLauncher->Size = Drawing::Size(140, 25); btnExportLauncher->Text = L"导出启动器exe";
    gbE->Controls->AddRange(gcnew array<Control^>{ txtExportDir, btnBrowseExportDir, txtExportOut, btnBrowseExportOut, txtSplit, btnExport,
        lRun, txtExeRun, btnExportSfx, btnExportLauncher });
    pnlGame->Controls->Add(gbE);
    StyleBtn(btnBrowseExportDir, false); StyleBtn(btnBrowseExportOut, false); StyleBtn(btnExport, true);
    StyleBtn(btnExportSfx, true); StyleBtn(btnExportLauncher, true);
    btnBrowseExportDir->Click += gcnew EventHandler(this, &MainForm::btnBrowseExportDir_Click);
    btnBrowseExportOut->Click += gcnew EventHandler(this, &MainForm::btnBrowseExportOut_Click);
    btnExport->Click += gcnew EventHandler(this, &MainForm::btnExport_Click);
    btnExportSfx->Click += gcnew EventHandler(this, &MainForm::btnExportSfx_Click);
    btnExportLauncher->Click += gcnew EventHandler(this, &MainForm::btnExportLauncher_Click);

    y += 124;

    // ============ 翻译流水线 ============
    GroupBox^ gbT = MakeGroup(L"翻译流水线 (提取对话 → AI翻译 → 生成翻译mod)", 12, y, 736, 112);
    txtTransDir = gcnew TextBox();    txtTransDir->Location = Drawing::Point(14, 28);  txtTransDir->Size = Drawing::Size(280, 23); txtTransDir->Font = UI::FONT;
    btnBrowseTransDir = gcnew Button(); btnBrowseTransDir->Location = Drawing::Point(299, 26); btnBrowseTransDir->Size = Drawing::Size(60, 25); btnBrowseTransDir->Text = L"浏览";
    txtTransCsv = gcnew TextBox();    txtTransCsv->Location = Drawing::Point(364, 28); txtTransCsv->Size = Drawing::Size(246, 23); txtTransCsv->Font = UI::FONT;
    btnBrowseTransCsv = gcnew Button(); btnBrowseTransCsv->Location = Drawing::Point(615, 26); btnBrowseTransCsv->Size = Drawing::Size(60, 25); btnBrowseTransCsv->Text = L"浏览";
    txtTransLang = gcnew TextBox();   txtTransLang->Location = Drawing::Point(14, 60); txtTransLang->Size = Drawing::Size(60, 23); txtTransLang->Font = UI::FONT; txtTransLang->Text = L"zh";
    btnTransExtract = gcnew Button(); btnTransExtract->Location = Drawing::Point(84, 58); btnTransExtract->Size = Drawing::Size(110, 25); btnTransExtract->Text = L"提取对话";
    btnTransAI = gcnew Button();      btnTransAI->Location = Drawing::Point(199, 58); btnTransAI->Size = Drawing::Size(110, 25); btnTransAI->Text = L"AI翻译";
    btnTransBuild = gcnew Button();   btnTransBuild->Location = Drawing::Point(314, 58); btnTransBuild->Size = Drawing::Size(130, 25); btnTransBuild->Text = L"生成翻译mod";
    gbT->Controls->AddRange(gcnew array<Control^>{ txtTransDir, btnBrowseTransDir, txtTransCsv, btnBrowseTransCsv,
        txtTransLang, btnTransExtract, btnTransAI, btnTransBuild });
    pnlGame->Controls->Add(gbT);
    StyleBtn(btnBrowseTransDir, false); StyleBtn(btnBrowseTransCsv, false);
    StyleBtn(btnTransExtract, true); StyleBtn(btnTransAI, true); StyleBtn(btnTransBuild, true);
    btnBrowseTransDir->Click += gcnew EventHandler(this, &MainForm::btnBrowseTransDir_Click);
    btnBrowseTransCsv->Click  += gcnew EventHandler(this, &MainForm::btnBrowseTransCsv_Click);
    btnTransExtract->Click    += gcnew EventHandler(this, &MainForm::btnTransExtract_Click);
    btnTransAI->Click         += gcnew EventHandler(this, &MainForm::btnTransAI_Click);
    btnTransBuild->Click      += gcnew EventHandler(this, &MainForm::btnTransBuild_Click);

    y += 124;

    // ============ 日志 ============
    GroupBox^ gbLog = MakeGroup(L"日志 (详细见 patcher.log)", 12, y, 736, 140);
    txtLog = gcnew TextBox();
    txtLog->Location = Drawing::Point(14, 24); txtLog->Size = Drawing::Size(648, 100);
    txtLog->Multiline = true; txtLog->ScrollBars = ScrollBars::Vertical; txtLog->ReadOnly = true;
    txtLog->BorderStyle = BorderStyle::None;
    txtLog->BackColor = UI::CONSOLE_B;
    txtLog->ForeColor = UI::CONSOLE_F;
    txtLog->Font = UI::FONT_MONO;
    btnClearLog = gcnew Button(); btnClearLog->Location = Drawing::Point(668, 54); btnClearLog->Size = Drawing::Size(60, 28); btnClearLog->Text = L"清空";
    gbLog->Controls->AddRange(gcnew array<Control^>{ txtLog, btnClearLog });
    pnlGame->Controls->Add(gbLog);
    StyleBtn(btnClearLog, false);
    btnClearLog->Click += gcnew EventHandler(this, &MainForm::btnClearLog_Click);
}

/* ===================== 设置 ===================== */
void MainForm::BuildSettings()
{
    pnlSettings = gcnew Panel();
    pnlSettings->Location = Drawing::Point(176, 8);
    pnlSettings->Size = Drawing::Size(760, 724);
    pnlSettings->AutoScroll = true;
    pnlSettings->BackColor = Drawing::Color::White;

    // ---- AI 翻译配置 ----
    GroupBox^ gbA = MakeGroup(L"AI 翻译配置（translate ai 使用）", 12, 12, 736, 158);
    Label^ lBase = MakeLabel(L"Base URL", 14, 28, 80, 23, false);
    txtAiBase = gcnew TextBox();   txtAiBase->Location = Drawing::Point(100, 26); txtAiBase->Size = Drawing::Size(440, 23); txtAiBase->Font = UI::FONT;
    txtAiBase->Text = L"https://api.openai.com/v1";
    Label^ lKey = MakeLabel(L"API Key", 14, 60, 80, 23, false);
    txtAiKey = gcnew TextBox();    txtAiKey->Location = Drawing::Point(100, 58); txtAiKey->Size = Drawing::Size(440, 23); txtAiKey->Font = UI::FONT;
    txtAiKey->UseSystemPasswordChar = true;
    Label^ lModel = MakeLabel(L"模型", 14, 92, 80, 23, false);
    txtAiModel = gcnew TextBox();  txtAiModel->Location = Drawing::Point(100, 90); txtAiModel->Size = Drawing::Size(220, 23); txtAiModel->Font = UI::FONT;
    txtAiModel->Text = L"gpt-4o-mini";
    Label^ lTip = MakeLabel(L"也可通过环境变量 RP_AI_API_KEY / RP_AI_BASE_URL / RP_AI_MODEL 设置；支持任意 OpenAI 兼容服务（DeepSeek、通义等）。", 14, 124, 700, 28, true);
    gbA->Controls->AddRange(gcnew array<Control^>{ lBase, txtAiBase, lKey, txtAiKey, lModel, txtAiModel, lTip });
    pnlSettings->Controls->Add(gbA);

    // ---- 关于 ----
    GroupBox^ gbAbout = MakeGroup(L"关于", 12, 186, 736, 210);
    lblAbout = gcnew Label();
    lblAbout->Location = Drawing::Point(14, 26); lblAbout->Size = Drawing::Size(700, 150);
    lblAbout->Font = UI::FONT; lblAbout->ForeColor = UI::TEXT;
    lblAbout->Text = gcnew String("Ren'Py Patcher  v") + gcnew String(RPATCHER_VERSION_STRING) + L"\n\n"
        L"一个 Ren'Py 游戏资源包处理工具：解包、注入 mod、来源声明、导出，\n"
        L"以及内置对话翻译流水线（提取 → AI / 人工翻译 → 生成 Ren'Py 翻译 mod）。\n\n"
        L"· 所有操作日志：同目录 patcher.log\n"
        L"· 游戏别名表：patcher_games.json（位于程序目录下）\n"
        L"· 底层复用同一套核心 C 库，GUI 与 CLI 行为一致。";
    lnkProject = gcnew LinkLabel();
    lnkProject->Location = Drawing::Point(14, 180); lnkProject->Size = Drawing::Size(400, 22);
    lnkProject->Text = L"在 GitHub 上查看项目 / 下载更新  →";
    lnkProject->Font = UI::FONT_B;
    lnkProject->LinkColor = UI::ACCENT;
    lnkProject->ActiveLinkColor = UI::ACCENT_D;
    gbAbout->Controls->AddRange(gcnew array<Control^>{ lblAbout, lnkProject });
    pnlSettings->Controls->Add(gbAbout);
    lnkProject->LinkClicked += gcnew LinkLabelLinkClickedEventHandler(this, &MainForm::lnkProject_Click);

    // ---- 检查更新 ----
    GroupBox^ gbUpd = MakeGroup(L"检查更新（按构建号比较，自动对比 GitHub 上的最新构建）", 12, 412, 736, 124);
    btnCheckUpdate = gcnew Button();
    btnCheckUpdate->Location = Drawing::Point(14, 30); btnCheckUpdate->Size = Drawing::Size(120, 30);
    btnCheckUpdate->Text = L"检查更新";
    lblUpdateStatus = gcnew Label();
    lblUpdateStatus->Location = Drawing::Point(146, 36); lblUpdateStatus->Size = Drawing::Size(560, 26);
    lblUpdateStatus->Font = UI::FONT; lblUpdateStatus->ForeColor = UI::MUTE;
    lblUpdateStatus->Text = String::Format(L"当前构建号 {0}（将在启动时自动检查更新）", (int)RPATCHER_BUILD_NUM);
    chkUpdateBeta = gcnew CheckBox();
    chkUpdateBeta->Location = Drawing::Point(14, 66); chkUpdateBeta->Size = Drawing::Size(680, 24);
    chkUpdateBeta->Font = UI::FONT; chkUpdateBeta->ForeColor = UI::TEXT;
    chkUpdateBeta->Text = L"接收测试版更新（不勾选则只提示正式版）";
    chkUpdateBeta->Checked = false;
    gbUpd->Controls->AddRange(gcnew array<Control^>{ btnCheckUpdate, lblUpdateStatus, chkUpdateBeta });
    pnlSettings->Controls->Add(gbUpd);
    StyleBtn(btnCheckUpdate, true);
    btnCheckUpdate->Click += gcnew EventHandler(this, &MainForm::btnCheckUpdate_Click);
    chkUpdateBeta->CheckedChanged += gcnew EventHandler(this, &MainForm::chkUpdateBeta_CheckedChanged);
}

/* ===================== 辅助 ===================== */
void MainForm::AppendLog(String^ msg)
{
    txtLog->AppendText(msg + L"\r\n");
    txtLog->ScrollToCaret();
}

void MainForm::SetStatus(String^ msg, bool)
{
    if (statusLabel) statusLabel->Text = msg;
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

void MainForm::RunOp(OpType type, Button^ btn, String^ opName, String^ a, String^ b, String^ c, String^ d)
{
    RunOp(type, btn, opName, a, b, c, d, nullptr, nullptr, nullptr);
}
void MainForm::RunOp(OpType type, Button^ btn, String^ opName, String^ a, String^ b, String^ c, String^ d, String^ base, String^ key, String^ model)
{
    TaskCtx^ ctx = gcnew TaskCtx();
    ctx->Type = type; ctx->a = a; ctx->b = b; ctx->c = c; ctx->d = d;
    ctx->base = base; ctx->key = key; ctx->model = model;
    ctx->opName = opName; ctx->btn = btn; ctx->ok = false; ctx->rc = 0;
    if (btn) btn->Enabled = false;
    _activeBtn = btn;
    this->UseWaitCursor = true;
    SetStatus(L"处理中：" + opName + L" …", true);
    bgTask->RunWorkerAsync(ctx);
}

String^ MainForm::ResolveGameRpa(String^ token)
{
    String^ t = token->Trim();
    std::string in = ToUtf8(t);
    if (!is_zip_file(in.c_str())) return t;
    String^ tmp = System::IO::Path::Combine(System::IO::Path::GetTempPath(),
                    L"renpy_patcher_gui_" + DateTime::Now.Ticks.ToString());
    char rpa[2048]; rpa[0] = 0;
    int rc = resolve_game_source(in.c_str(), ToUtf8(tmp).c_str(), rpa, sizeof(rpa));
    if (rc == RP_OK && rpa[0]) return gcnew String(rpa);
    return t;
}

String^ MainForm::FindStub()
{
    String^ dir = System::IO::Path::GetDirectoryName(System::Windows::Forms::Application::ExecutablePath);
    String^ p = System::IO::Path::Combine(dir, L"sfx_stub.exe");
    if (System::IO::File::Exists(p)) return p;
    return nullptr;
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
    try {
        std::string name = ToUtf8(txtAlias->Text->Trim());
        std::string path = ToUtf8(txtGamePath->Text->Trim());
        if (name.empty() || path.empty()) { AppendLog(L"[游戏管理] 别名与路径均不能为空"); return; }
        if (is_zip_file(path.c_str())) {
            String^ tmp = System::IO::Path::Combine(System::IO::Path::GetTempPath(), L"rpg_import_" + DateTime::Now.Ticks.ToString());
            char rpa[2048]; rpa[0] = 0;
            int zrc = resolve_game_source(path.c_str(), ToUtf8(tmp).c_str(), rpa, sizeof(rpa));
            if (zrc != RP_OK || rpa[0] == 0) { AppendLog(L"[游戏管理] 压缩包内未找到 .rpa 资源包"); return; }
            path = std::string(rpa);
        }
        int rc = game_add(name.c_str(), path.c_str(), "");
        if (rc == RP_OK) { AppendLog(gcnew String("已添加游戏: ") + txtAlias->Text->Trim()); RefreshGames(); }
        else ReportFail(L"游戏管理-添加", rc);
    }
    catch (Exception^ e) { AppendLog(L"❌ 异常（已拦截，程序未崩溃）: " + e->Message); }
    catch (...) { AppendLog(L"❌ 发生未知底层异常（已拦截，程序未崩溃）"); }
    SyncLog();
}

void MainForm::btnRemoveGame_Click(Object^, EventArgs^)
{
    try {
        if (lstGames->SelectedIndex < 0) { AppendLog(L"[游戏管理] 请先在列表选择要删除的游戏"); return; }
        String^ item = safe_cast<String^>(lstGames->SelectedItem);
        int sp = item->IndexOf(L"  ->  ");
        String^ name = (sp > 0) ? item->Substring(0, sp) : item;
        int rc = game_remove(ToUtf8(name).c_str());
        if (rc == RP_OK) { AppendLog(gcnew String("已删除游戏: ") + name); RefreshGames(); }
        else ReportFail(L"游戏管理-删除", rc);
    }
    catch (Exception^ e) { AppendLog(L"❌ 异常（已拦截，程序未崩溃）: " + e->Message); }
    catch (...) { AppendLog(L"❌ 发生未知底层异常（已拦截，程序未崩溃）"); }
    SyncLog();
}

void MainForm::btnRefreshGames_Click(Object^, EventArgs^)
{
    RefreshGames();
    AppendLog(L"[游戏管理] 已刷新");
}

void MainForm::btnUnpack_Click(Object^, EventArgs^)
{
    if (!CheckFile(txtUnpackIn, L"解包-输入")) return;
    String^ rpa = ResolveGameRpa(txtUnpackIn->Text->Trim());
    RunOp(OpType::Unpack, btnUnpack, L"解包", rpa, txtUnpackOut->Text->Trim(), nullptr, nullptr);
}

void MainForm::btnBuild_Click(Object^, EventArgs^)
{
    if (!CheckFile(txtBuildIn, L"注入-游戏")) return;
    if (!CheckDir(txtModDir, L"注入-mod目录")) return;
    String^ rpa = ResolveGameRpa(txtBuildIn->Text->Trim());
    RunOp(OpType::Build, btnBuild, L"注入", rpa, txtModDir->Text->Trim(), txtBuildOut->Text->Trim(), nullptr);
}

void MainForm::btnCredit_Click(Object^, EventArgs^)
{
    if (!CheckFile(txtCreditIn, L"声明-输入")) return;
    RunOp(OpType::Credit, btnCredit, L"来源声明", txtCreditIn->Text->Trim(), txtCreditOut->Text->Trim(), nullptr, nullptr);
}

void MainForm::btnExport_Click(Object^, EventArgs^)
{
    if (!CheckDir(txtExportDir, L"导出-目录")) return;
    RunOp(OpType::Export, btnExport, L"导出", txtExportDir->Text->Trim(), txtExportOut->Text->Trim(), txtSplit->Text->Trim(), nullptr);
}
void MainForm::btnExportSfx_Click(Object^, EventArgs^)
{
    if (!CheckDir(txtExportDir, L"导出-目录")) return;
    String^ out = txtExportOut->Text->Trim();
    if (String::IsNullOrWhiteSpace(out)) out = L"setup.exe";
    RunOp(OpType::ExportSfx, btnExportSfx, L"导出自解压exe", txtExportDir->Text->Trim(), out, txtExeRun->Text->Trim(), nullptr);
}
void MainForm::btnExportLauncher_Click(Object^, EventArgs^)
{
    if (!CheckDir(txtExportDir, L"导出-目录")) return;
    String^ run = txtExeRun->Text->Trim();
    if (String::IsNullOrWhiteSpace(run)) { AppendLog(L"[导出] 启动器模式需填写启动命令"); return; }
    String^ out = txtExportOut->Text->Trim();
    if (String::IsNullOrWhiteSpace(out)) out = L"setup.exe";
    RunOp(OpType::ExportLauncher, btnExportLauncher, L"导出启动器exe", txtExportDir->Text->Trim(), out, run, nullptr);
}

void MainForm::btnClearLog_Click(Object^, EventArgs^)
{
    txtLog->Clear();
}

// 浏览按钮
void MainForm::btnBrowseGame_Click(Object^, EventArgs^)
{
    BrowseFile(txtGamePath, L"Ren'Py 资源包 (*.rpa)|*.rpa|所有文件 (*.*)|*.*");
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

void MainForm::SyncLog()
{
    try {
        String^ p = "patcher.log";
        if (!System::IO::File::Exists(p)) return;
        System::IO::FileInfo^ fi = gcnew System::IO::FileInfo(p);
        long cur = fi->Length;
        if (cur < g_logPos) g_logPos = 0;          // 日志被重建，从头读
        if (cur == g_logPos) return;
        System::IO::FileStream^ fs = gcnew System::IO::FileStream(p,
            System::IO::FileMode::Open, System::IO::FileAccess::Read, System::IO::FileShare::ReadWrite);
        fs->Seek(g_logPos, System::IO::SeekOrigin::Begin);
        System::IO::StreamReader^ sr = gcnew System::IO::StreamReader(fs);
        String^ tail = sr->ReadToEnd();
        if (tail != nullptr && tail->Length > 0) {
            txtLog->AppendText(tail);
            txtLog->ScrollToCaret();
        }
        g_logPos = fi->Length;
        fs->Close();
    }
    catch (...) { /* 日志读取异常不影响主流程 */ }
}

void MainForm::ReportFail(String^ op, int rc)
{
    AppendLog(FailMsg(op, rc));
}

bool MainForm::CheckFile(TextBox^ tb, String^ label)
{
    String^ t = (tb != nullptr) ? tb->Text : nullptr;
    if (String::IsNullOrWhiteSpace(t)) { AppendLog(L"[" + label + L"] 路径为空"); return false; }
    if (!System::IO::File::Exists(t))  { AppendLog(L"[" + label + L"] 文件不存在: " + t); return false; }
    return true;
}

bool MainForm::CheckDir(TextBox^ tb, String^ label)
{
    String^ t = (tb != nullptr) ? tb->Text : nullptr;
    if (String::IsNullOrWhiteSpace(t))      { AppendLog(L"[" + label + L"] 路径为空"); return false; }
    if (!System::IO::Directory::Exists(t))   { AppendLog(L"[" + label + L"] 目录不存在: " + t); return false; }
    return true;
}

// ---- 后台任务：文件操作 ----
void MainForm::bgTask_DoWork(Object^, DoWorkEventArgs^ e)
{
    TaskCtx^ c = (TaskCtx^)e->Argument;
    c->ok = false; c->rc = 0;
    try {
        std::string a = (c->a && c->a->Length) ? ToUtf8(c->a) : std::string();
        std::string b = (c->b && c->b->Length) ? ToUtf8(c->b) : std::string();
        std::string cc = (c->c && c->c->Length) ? ToUtf8(c->c) : std::string();
        std::string d = (c->d && c->d->Length) ? ToUtf8(c->d) : std::string();
        int rc = 0;

        switch (c->Type) {
        case OpType::Unpack: {
            std::string out = b.empty() ? "out" : b;
            rc = rpa_v3_unpack(a.c_str(), out.c_str());
            c->msg = L"[解包] 完成 ✅";
            break;
        }
        case OpType::Build: {
            std::string out = cc.empty() ? "patched.rpa" : cc;
            rc = direct_inject_build(a.c_str(), b.c_str(), out.c_str());
            c->msg = L"[注入] 完成 ✅ (已自动写入来源声明)";
            break;
        }
        case OpType::Credit: {
            std::string out = b.empty() ? "credited.rpa" : b;
            rc = credit_add_to_rpa(a.c_str(), out.c_str(), a.c_str());
            c->msg = L"[声明] 完成 ✅";
            break;
        }
        case OpType::Export: {
            std::string out = b.empty() ? "patch.zip" : b;
            size_t split = ParseSize(gcnew String(d.c_str()));
            rc = export_dir(a.c_str(), out.c_str(), split, 1);
            c->msg = L"[导出] 完成 ✅";
            break;
        }
        case OpType::TransExtract: {
            std::string out = b.empty() ? "dialogs.json" : b;
            dialog_entry_t *arr = NULL; size_t cnt = 0;
            rc = extract_dialogs(a.c_str(), &arr, &cnt);
            if (rc == RP_OK) {
                rc = write_dialogs_json(arr, cnt, out.c_str());
                if (rc == RP_OK) c->msg = String::Format(L"[翻译] 提取完成 ✅ 共 {0} 条", (int)cnt);
            }
            if (arr) free_dialog_entries(arr, cnt);
            break;
        }
        case OpType::TransAI: {
            String^ inS = gcnew String(a.c_str());
            int dot = inS->LastIndexOf('.');
            String^ outJson = (dot >= 0) ? inS->Substring(0, dot) + L"_translated.json" : inS + L"_translated.json";
            std::string outPath = ToUtf8(outJson);
            dialog_entry_t *arr = NULL; size_t cnt = 0;
            rc = read_dialogs_json(a.c_str(), &arr, &cnt);
            if (rc == RP_OK) {
                const char **srcs = new const char*[cnt];
                char **tgts = new char*[cnt];
                for (size_t i = 0; i < cnt; i++) { srcs[i] = arr[i].source; tgts[i] = NULL; }
                std::string burl  = (c->base && c->base->Length)  ? ToUtf8(c->base)  : std::string();
                std::string akey  = (c->key  && c->key->Length)   ? ToUtf8(c->key)   : std::string();
                std::string amodel= (c->model&& c->model->Length) ? ToUtf8(c->model) : std::string();
                rc = ai_translate_batch(srcs, tgts, cnt, "auto", "Chinese",
                                        burl.empty()?nullptr:burl.c_str(),
                                        akey.empty()?nullptr:akey.c_str(),
                                        amodel.empty()?nullptr:amodel.c_str());
                if (rc == RP_OK) {
                    for (size_t i = 0; i < cnt; i++) {
                        if (tgts[i] && *tgts[i]) { free(arr[i].target); arr[i].target = tgts[i]; }
                        else free(tgts[i]);
                    }
                    write_dialogs_json(arr, cnt, outPath.c_str());
                    c->msg = L"[翻译] AI 翻译完成 ✅";
                    c->outPath = outJson;
                }
                delete[] srcs; delete[] tgts;
            }
            if (arr) free_dialog_entries(arr, cnt);
            break;
        }
        case OpType::ExportSfx: {
            std::string dir = a.empty() ? "out" : a;
            std::string out = b.empty() ? "setup.exe" : b;
            String^ stub = FindStub();
            std::string stub8 = (stub && stub->Length) ? ToUtf8(stub) : std::string();
            rc = sfx_build(stub8.empty()?nullptr:stub8.c_str(), dir.c_str(), out.c_str(), nullptr, 1);
            c->msg = L"[导出] 自解压 exe 完成 ✅";
            break;
        }
        case OpType::ExportLauncher: {
            std::string dir = a.empty() ? "out" : a;
            std::string out = b.empty() ? "setup.exe" : b;
            std::string run = c.empty() ? std::string() : c;
            String^ stub = FindStub();
            std::string stub8 = (stub && stub->Length) ? ToUtf8(stub) : std::string();
            rc = sfx_build(stub8.empty()?nullptr:stub8.c_str(), dir.c_str(), out.c_str(), run.c_str(), 1);
            c->msg = L"[导出] 启动器 exe 完成 ✅";
            break;
        }
        case OpType::TransBuild: {
            String^ inS = gcnew String(a.c_str());
            String^ lang = (d.empty() ? L"zh" : gcnew String(d.c_str()));
            int sp = inS->LastIndexOf('\\'); if (sp < 0) sp = inS->LastIndexOf('/');
            String^ csvDir = (sp >= 0) ? inS->Substring(0, sp + 1) : L"";
            String^ modDir = csvDir + L"translation_mod";
            std::string mdir = ToUtf8(modDir);
            std::string lang8 = ToUtf8(lang);
            dialog_entry_t *arr = NULL; size_t cnt = 0;
            rc = read_dialogs_json(a.c_str(), &arr, &cnt);
            if (rc == RP_OK) {
                rc = build_translation_mod(arr, cnt, mdir.c_str(), lang8.c_str());
                if (rc == RP_OK) c->msg = String::Format(L"[翻译] 生成翻译 mod 完成 ✅ -> {0}tl/{1}/", modDir, lang);
            }
            if (arr) free_dialog_entries(arr, cnt);
            break;
        }
        }
        c->rc = rc;
        c->ok = (rc == RP_OK);
    }
    catch (Exception^ ex) { c->exMsg = ex->Message; }
    catch (...) { c->exMsg = L"未知底层异常"; }
    e->Result = c;
}

void MainForm::bgTask_RunWorkerCompleted(Object^, RunWorkerCompletedEventArgs^ e)
{
    this->UseWaitCursor = false;
    TaskCtx^ c = (TaskCtx^)e->Result;
    if (c->btn) c->btn->Enabled = true;
    _activeBtn = nullptr;

    if (!String::IsNullOrEmpty(c->exMsg)) {
        AppendLog(L"❌ 异常（已拦截，程序未崩溃）: " + c->exMsg);
        SetStatus(L"异常：" + c->opName, false);
    } else if (c->ok) {
        AppendLog(c->msg);
        SetStatus(L"完成：" + c->opName, false);
    } else {
        AppendLog(FailMsg(c->opName, c->rc));
        SetStatus(L"失败：" + c->opName, false);
    }

    if (c->Type == OpType::TransAI && c->ok && !String::IsNullOrEmpty(c->outPath)) {
        txtTransCsv->Text = c->outPath;   // 便于下一步直接「生成翻译mod」
    }
    SyncLog();
}

// ---- 翻译流水线 ----

void MainForm::btnBrowseTransDir_Click(Object^, EventArgs^) { BrowseFolder(txtTransDir); }
void MainForm::btnBrowseTransCsv_Click(Object^, EventArgs^)
{
    BrowseFile(txtTransCsv, L"JSON 文件 (*.json)|*.json|所有文件 (*.*)|*.*");
}

void MainForm::btnTransExtract_Click(Object^, EventArgs^)
{
    if (!CheckDir(txtTransDir, L"翻译-解包目录")) return;
    String^ csv = txtTransCsv->Text->Trim();
    if (String::IsNullOrWhiteSpace(csv)) { AppendLog(L"[翻译] 请先填写 JSON 输出路径"); return; }
    RunOp(OpType::TransExtract, btnTransExtract, L"提取对话", txtTransDir->Text->Trim(), csv, nullptr, nullptr);
}

void MainForm::btnTransAI_Click(Object^, EventArgs^)
{
    if (!CheckFile(txtTransCsv, L"翻译-JSON")) return;
    String^ csv = txtTransCsv->Text->Trim();
    RunOp(OpType::TransAI, btnTransAI, L"AI翻译", csv, nullptr, nullptr, nullptr,
          txtAiBase->Text->Trim(), txtAiKey->Text->Trim(), txtAiModel->Text->Trim());
}

void MainForm::btnTransBuild_Click(Object^, EventArgs^)
{
    if (!CheckFile(txtTransCsv, L"翻译-JSON")) return;
    String^ csv  = txtTransCsv->Text->Trim();
    String^ lang = txtTransLang->Text->Trim();
    if (String::IsNullOrWhiteSpace(lang)) lang = L"zh";
    RunOp(OpType::TransBuild, btnTransBuild, L"生成翻译mod", csv, nullptr, nullptr, lang);
}

// ---- 设置持久化 ----

void MainForm::lnkProject_Click(Object^, LinkLabelLinkClickedEventArgs^)
{
    try { System::Diagnostics::Process::Start(L"https://github.com/Zz131417/renpy_patcher"); }
    catch (...) {}
}

String^ MainForm::ConfigPath()
{
    String^ dir = Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData);
    return System::IO::Path::Combine(dir, L"RenPyPatcher\\gui.cfg");
}

void MainForm::LoadConfig()
{
    try {
        String^ p = ConfigPath();
        if (!System::IO::File::Exists(p)) return;
        array<String^>^ lines = System::IO::File::ReadAllLines(p);
        for each (String^ line in lines) {
            if (line->StartsWith(L"updateBeta=")) {
                String^ v = line->Substring(11)->Trim()->ToLower();
                chkUpdateBeta->Checked = (v == L"1" || v == L"true");
            }
        }
    }
    catch (...) { /* 配置读取失败不影响启动 */ }
}

void MainForm::SaveConfig()
{
    try {
        String^ p = ConfigPath();
        String^ dir = System::IO::Path::GetDirectoryName(p);
        if (!System::IO::Directory::Exists(dir)) System::IO::Directory::CreateDirectory(dir);
        array<String^>^ lines = { String::Format(L"updateBeta={0}", chkUpdateBeta->Checked ? 1 : 0) };
        System::IO::File::WriteAllLines(p, lines);
    }
    catch (...) { /* 配置写入失败忽略 */ }
}

void MainForm::chkUpdateBeta_CheckedChanged(Object^, EventArgs^)
{
    SaveConfig();   // 开关变化即持久化
}

/* ===================== 检查更新 ===================== */
void MainForm::btnCheckUpdate_Click(Object^, EventArgs^)
{
    if (bgUpdate->IsBusy) { lblUpdateStatus->Text = L"正在检查更新…"; return; }
    lblUpdateStatus->Text = L"正在检查更新…";
    bgUpdate->RunWorkerAsync(true);   // true=手动（总是弹窗反馈）
}

void MainForm::bgUpdate_DoWork(Object^, DoWorkEventArgs^ e)
{
    bool manual = (e->Argument != nullptr) ? (bool)e->Argument : false;
    try {
        String^ url = L"https://raw.githubusercontent.com/Zz131417/renpy_patcher/latest/version.json";
        WebClient^ wc = gcnew WebClient();
        wc->Headers->Add("User-Agent", "RenPyPatcher-GUI");
        String^ json = wc->DownloadString(url);
        int latestBuild = JsonInt(json, "build");
        String^ ver  = JsonField(json, "version");
        String^ tag  = JsonField(json, "tag");
        String^ link = JsonField(json, "url");
        String^ pre = JsonField(json, "prerelease");
        bool latestPre = (pre != nullptr) && (pre->IndexOf("true") >= 0);
        int cur = RPATCHER_BUILD_NUM;
        bool has = (latestBuild > cur) && (latestBuild > 0);
        e->Result = gcnew UpdateResult(has, manual, ver, tag, link, latestBuild, cur, latestPre, nullptr);
    }
    catch (Exception^ ex) {
        e->Result = gcnew UpdateResult(false, manual, nullptr, nullptr, nullptr, 0, RPATCHER_BUILD_NUM, false, ex->Message);
    }
}

void MainForm::bgUpdate_RunWorkerCompleted(Object^, RunWorkerCompletedEventArgs^ e)
{
    if (e->Error != nullptr || e->Result == nullptr) {
        lblUpdateStatus->Text = L"检查更新失败（网络不可用或可稍后重试）";
        return;
    }
    UpdateResult^ r = (UpdateResult^)e->Result;
    if (!String::IsNullOrEmpty(r->Error)) {
        lblUpdateStatus->Text = L"检查更新失败（" + r->Error + L"）";
        return;
    }
    bool showUpdate = r->HasUpdate && (chkUpdateBeta->Checked || !r->Prerelease);
    if (showUpdate) {
        lblUpdateStatus->Text = String::Format(L"发现新版本 v{0}（build {1}，当前 {2}）",
                                               r->Version, r->LatestBuild, r->CurBuild);
        System::Windows::Forms::DialogResult dr = MessageBox::Show(
            String::Format(L"发现新版本 v{0}（build {1}）\n当前版本 build {2}。\n\n是否前往下载？",
                          r->Version, r->LatestBuild, r->CurBuild),
            L"检查更新", MessageBoxButtons::YesNo, MessageBoxIcon::Information);
        if (dr == System::Windows::Forms::DialogResult::Yes && !String::IsNullOrEmpty(r->Url)) {
            try { System::Diagnostics::Process::Start(r->Url); } catch (...) {}
        }
    } else {
        if (r->HasUpdate && !chkUpdateBeta->Checked && r->Prerelease) {
            lblUpdateStatus->Text = String::Format(L"已是最新正式版（已关闭测试版更新通道，build {0}）", r->CurBuild);
        } else {
            lblUpdateStatus->Text = String::Format(L"已是最新（build {0}）", r->CurBuild);
        }
        if (r->Manual) MessageBox::Show(L"当前已是最新版本。", L"检查更新",
                                         MessageBoxButtons::OK, MessageBoxIcon::Information);
    }
}

} // namespace RenPyPatcherGUI
