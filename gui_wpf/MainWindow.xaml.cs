using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using Microsoft.Win32;

namespace RenPyPatcherGui
{
    // 已登记游戏的数据项（绑定到列表）
    public class GameEntry
    {
        public string Name { get; set; }
        public string Path { get; set; }
        public string Note { get; set; }
    }

    public partial class MainWindow : Window
    {
        // 保存委托引用，防止被 GC 回收
        private Native.LogCallback _logCb;
        private Native.ProgressCallback _progCb;
        private Native.GameCallback _gameCb;
        private bool _busy = false;

        // 游戏列表数据源
        private ObservableCollection<GameEntry> _games = new ObservableCollection<GameEntry>();
        private List<GameEntry> _pendingGames = new List<GameEntry>();

        // 数据目录（位于可执行文件目录下，便于便携）：game/ 存游戏，settings/ 存登记表与界面设置
        private string _baseDir, _gameDir, _settingsDir;

        public MainWindow()
        {
            InitializeComponent();
            lstGames.ItemsSource = _games;
            Loaded += MainWindow_Loaded;
        }

        private void EnsureDataDirs()
        {
            _baseDir = AppContext.BaseDirectory;
            _gameDir = System.IO.Path.Combine(_baseDir, "game");
            _settingsDir = System.IO.Path.Combine(_baseDir, "settings");
            // 首次启动（或目录被删）时新建 game/ 与 settings/
            System.IO.Directory.CreateDirectory(_gameDir);
            System.IO.Directory.CreateDirectory(_settingsDir);
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            // 初始化核心 + 回调
            EnsureDataDirs();
            Native.rp_game_init(System.IO.Path.Combine(_settingsDir, "games.txt"));
            _logCb = LogCallbackImpl;
            _progCb = ProgressCallbackImpl;
            Native.rp_set_log_callback(_logCb, IntPtr.Zero);
            Native.rp_set_progress_callback(_progCb, IntPtr.Zero);
            Native.rp_set_log_level(0);

            _gameCb = GameCallbackImpl;

            txtVer.Text = "版本 " + Native.rp_version_string();

            ApplySettings();
            RefreshGames();
            Log(1, "GUI 已就绪");
        }

        // ---------------- 设置应用 ----------------
        private void ApplySettings()
        {
            var s = App.Settings;
            FontFamily = new FontFamily(s.FontFamily);
            FontSize = s.FontSize;
            App.Current.Resources["CornerRadius"] = s.Round ? 8.0 : 0.0;
            ApplyBlur(s.Blur);
            chkRound.IsChecked = s.Round;
            chkAnim.IsChecked = s.Anim;
            chkBlur.IsChecked = s.Blur;
            cmbFont.Text = s.FontFamily;
            cmbFontSize.Text = s.FontSize.ToString();
        }

        private void ApplyBlur(bool on)
        {
            var bb = new Native.DWM_BLURBEHIND
            {
                dwFlags = 0x1 /*DWM_BB_ENABLE*/,
                fEnable = on,
                hRgnBlur = IntPtr.Zero,
                fTransitionOnMaximized = false
            };
            try { Native.DwmEnableBlurBehindWindow(new System.Windows.Interop.WindowInteropHelper(this).Handle, ref bb); }
            catch { }
        }

        // ---------------- 设置飞出面板（左上角）----------------
        private void BtnSettings_Click(object s, RoutedEventArgs e) => OpenSettings();
        private void BtnSettingsClose_Click(object s, RoutedEventArgs e) => CloseSettings();
        private void SettingsBackdrop_Click(object s, RoutedEventArgs e) => CloseSettings();

        private void OpenSettings()
        {
            settingsBackdrop.Visibility = Visibility.Visible;
            settingsPanel.Visibility = Visibility.Visible;
            if (App.Settings.Anim)
            {
                var a = new DoubleAnimation(280, TimeSpan.FromMilliseconds(200))
                { EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut } };
                settingsPanel.BeginAnimation(FrameworkElement.WidthProperty, a);
            }
            else
            {
                settingsPanel.Width = 280;
            }
        }

        private void CloseSettings()
        {
            if (App.Settings.Anim)
            {
                var a = new DoubleAnimation(0, TimeSpan.FromMilliseconds(180));
                a.Completed += (_, __) =>
                {
                    settingsPanel.Visibility = Visibility.Collapsed;
                    settingsBackdrop.Visibility = Visibility.Collapsed;
                };
                settingsPanel.BeginAnimation(FrameworkElement.WidthProperty, a);
            }
            else
            {
                settingsPanel.Width = 0;
                settingsPanel.Visibility = Visibility.Collapsed;
                settingsBackdrop.Visibility = Visibility.Collapsed;
            }
        }

        // ---------------- 回调 ----------------
        private void LogCallbackImpl(int level, IntPtr msg, IntPtr user)
        {
            string text = Native.PtrToUtf8(msg);
            Dispatcher.Invoke(() => Log(level, text));
        }

        private void ProgressCallbackImpl(int percent, IntPtr stage, IntPtr user)
        {
            string text = Native.PtrToUtf8(stage);
            Dispatcher.Invoke(() =>
            {
                pbProgress.Value = Math.Max(0, Math.Min(100, percent));
                if (!string.IsNullOrEmpty(text)) txtStatus.Text = text;
            });
        }

        private int GameCallbackImpl(IntPtr name, IntPtr path, IntPtr note, IntPtr user)
        {
            _pendingGames.Add(new GameEntry
            {
                Name = Native.PtrToUtf8(name),
                Path = Native.PtrToUtf8(path),
                Note = Native.PtrToUtf8(note)
            });
            return 0;
        }

        private void Log(int level, string msg)
        {
            if (string.IsNullOrEmpty(msg)) return;
            lstLog.Items.Add(msg);
            if (lstLog.Items.Count > 500) lstLog.Items.RemoveAt(0);
            lstLog.ScrollIntoView(lstLog.Items[lstLog.Items.Count - 1]);
        }

        // ---------------- 后台执行封装 ----------------
        private async void RunOp(string name, Func<int> work)
        {
            if (_busy) { Log(2, "上一项任务进行中，请稍候"); return; }
            _busy = true;
            txtStatus.Text = name + " 进行中…";
            pbProgress.Value = 0;
            int rc = RP_ERR_UNKNOWN;
            await Task.Run(() => { rc = work(); });
            if (rc == 0)
                Log(1, name + "：完成 ✅");
            else
                Log(3, name + "：失败 ❌ " + Native.PtrToUtf8(Native.rp_error_message()));
            txtStatus.Text = name + (rc == 0 ? " 完成" : " 失败");
            pbProgress.Value = 100;
            _busy = false;
        }
        private const int RP_ERR_UNKNOWN = -1;

        // ---------------- 游戏别名 ----------------
        private void RefreshGames()
        {
            _pendingGames.Clear();
            Native.rp_game_list(_gameCb, IntPtr.Zero);
            _games.Clear();
            foreach (var g in _pendingGames) _games.Add(g);
            bool hasGames = _games.Count > 0;
            // 空状态：未登记任何游戏时提示「请导入游戏」
            emptyState.Visibility = hasGames ? Visibility.Collapsed : Visibility.Visible;
            // 首次添加后：内联的「添加/更新」「删除选中」移到右下角浮动按钮
            btnAliasAdd.Visibility = hasGames ? Visibility.Collapsed : Visibility.Visible;
            btnAliasRemove.Visibility = hasGames ? Visibility.Collapsed : Visibility.Visible;
            btnAddFab.Visibility = hasGames ? Visibility.Visible : Visibility.Collapsed;
        }

        private void BtnAliasAdd_Click(object s, RoutedEventArgs e)
        {
            var name = txtAliasName.Text.Trim();
            var path = txtAliasPath.Text.Trim();
            if (name.Length == 0 || path.Length == 0) { Log(2, "名称与路径均必填"); return; }
            int rc = Native.rp_game_add(name, path, txtAliasNote.Text.Trim());
            Log(rc == 0 ? 1 : 3, rc == 0 ? $"已登记：{name}" : "登记失败");
            RefreshGames();
        }

        private void BtnAliasRemove_Click(object s, RoutedEventArgs e)
        {
            var sel = lstGames.SelectedItem as GameEntry;
            if (sel == null) return;
            int rc = Native.rp_game_remove(sel.Name);
            Log(rc == 0 ? 1 : 3, rc == 0 ? $"已删除：{sel.Name}" : "删除失败");
            RefreshGames();
        }

        // ---------------- 右下角浮动「添加游戏」：点击后展开内联表单并滚到顶部 ----------------
        private void BtnAddFab_Click(object s, RoutedEventArgs e)
        {
            // 重新显示内联表单的添加/删除按钮，方便填写；填写完点「添加 / 更新」即再次收起
            btnAliasAdd.Visibility = Visibility.Visible;
            btnAliasRemove.Visibility = Visibility.Visible;
            pageGame.ScrollToTop();
            txtAliasName.Focus();
        }

        // ---------------- 从压缩包导入：解压进 game/<名称>/ 并登记（仅此类游戏进入 game/） ----------------
        private void BtnImportZip_Click(object s, RoutedEventArgs e)
        {
            var d = new OpenFileDialog { Filter = "压缩包 (*.zip)|*.zip", CheckFileExists = true };
            if (d.ShowDialog() != true) return;
            string zip = d.FileName;
            string name = txtImportName.Text.Trim();
            if (name.Length == 0) name = System.IO.Path.GetFileNameWithoutExtension(zip);
            if (name.Length == 0) { Log(2, "无法取得游戏名"); return; }
            string dest = System.IO.Path.Combine(_gameDir, name);
            string captured = name;
            RunOp("从压缩包导入", () =>
            {
                System.IO.Directory.CreateDirectory(dest);
                ZipFile.ExtractToDirectory(zip, dest, true);
                return Native.rp_game_add(captured, dest, "从压缩包导入");
            });
            txtImportName.Clear();
        }

        // ---------------- 拖动导入 ----------------
        private void MainWindow_DragOver(object sender, DragEventArgs e)
        {
            e.Effects = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
            e.Handled = true;
        }

        private void MainWindow_Drop(object sender, DragEventArgs e)
        {
            if (!e.Data.GetDataPresent(DataFormats.FileDrop)) return;
            e.Handled = true;
            var items = (string[])e.Data.GetData(DataFormats.FileDrop);
            foreach (var it in items) HandleDropItem(it);
        }

        private void HandleDropItem(string path)
        {
            if (Directory.Exists(path))
            {
                // 拖入文件夹：登记为外部路径游戏（不复制进 game/）
                string name = System.IO.Path.GetFileName(path.TrimEnd(System.IO.Path.DirectorySeparatorChar));
                if (name.Length == 0) { Log(2, "无法取得文件夹名"); return; }
                string captured = name, dir = path;
                RunOp("拖入文件夹（添加游戏）", () => Native.rp_game_add(captured, dir, "拖入文件夹"));
            }
            else if (File.Exists(path) && string.Equals(System.IO.Path.GetExtension(path), ".zip", StringComparison.OrdinalIgnoreCase))
            {
                // 拖入 zip：解压进 game/<名称>/
                string name = System.IO.Path.GetFileNameWithoutExtension(path);
                string dest = System.IO.Path.Combine(_gameDir, name);
                string captured = name, src = path;
                RunOp("拖入压缩包（导入）", () =>
                {
                    System.IO.Directory.CreateDirectory(dest);
                    ZipFile.ExtractToDirectory(src, dest, true);
                    return Native.rp_game_add(captured, dest, "拖入压缩包导入");
                });
            }
            else
            {
                Log(2, "不支持的拖入类型：" + path);
            }
        }

        // ---------------- 列表项：管理 / 启动 ----------------
        private void BtnGameManage_Click(object s, RoutedEventArgs e)
        {
            var name = (s as Button)?.Tag as string;
            if (string.IsNullOrEmpty(name)) return;
            string path = Native.PtrToUtf8(Native.rp_game_resolve(name));
            // token 类字段填名称，目录类字段填路径，方便直接执行下方功能
            txtUnpackGame.Text = name;
            txtCreditGame.Text = name;
            txtTrDir.Text = path;
            txtBranchDir.Text = path;
            txtExportDir.Text = path;
            txtModTarget.Text = path;
            // 同时回填别名框，便于修改
            txtAliasName.Text = name;
            txtAliasPath.Text = path;
            Log(1, $"已为「{name}」填入操作参数，可直接执行下方功能");
        }

        private void BtnGameLaunch_Click(object s, RoutedEventArgs e)
        {
            var name = (s as Button)?.Tag as string;
            if (string.IsNullOrEmpty(name)) return;
            string raw = Native.PtrToUtf8(Native.rp_game_resolve(name));
            if (string.IsNullOrEmpty(raw)) { Log(2, "找不到游戏路径"); return; }
            try
            {
                if (Directory.Exists(raw))
                {
                    var exe = Directory.GetFiles(raw, "*.exe", SearchOption.TopDirectoryOnly)
                        .FirstOrDefault(x => !Path.GetFileName(x).ToLower().Contains("uninstall"));
                    if (exe != null) { Process.Start(exe); Log(1, "启动：" + exe); }
                    else { Process.Start("explorer.exe", raw); Log(1, "未找到 exe，已打开目录"); }
                }
                else if (File.Exists(raw))
                {
                    Process.Start(raw); Log(1, "启动：" + raw);
                }
                else { Log(2, "路径不存在：" + raw); }
            }
            catch (Exception ex) { Log(3, "启动失败：" + ex.Message); }
        }

        // ---------------- 文件/文件夹选择 ----------------
        private void PickFolder(TextBox tb)
        {
            var d = new System.Windows.Forms.FolderBrowserDialog { Description = "选择文件夹" };
            if (d.ShowDialog() == System.Windows.Forms.DialogResult.OK) tb.Text = d.SelectedPath;
        }
        private void PickFile(TextBox tb, string filter)
        {
            var d = new OpenFileDialog { Filter = filter, CheckFileExists = false };
            if (d.ShowDialog() == true) tb.Text = d.FileName;
        }

        private void BtnBrowseAlias_Click(object s, RoutedEventArgs e) => PickFolder(txtAliasPath);
        private void BtnBrowseUnpackOut_Click(object s, RoutedEventArgs e) => PickFolder(txtUnpackOut);
        private void BtnBrowseModDir_Click(object s, RoutedEventArgs e) => PickFolder(txtModDir);
        private void BtnBrowseExportDir_Click(object s, RoutedEventArgs e) => PickFolder(txtExportDir);
        private void BtnBrowseBranchDir_Click(object s, RoutedEventArgs e) => PickFolder(txtBranchDir);
        private void BtnBrowseTrDir_Click(object s, RoutedEventArgs e) => PickFolder(txtTrDir);

        // ---------------- 资源操作 ----------------
        private void BtnUnpack_Click(object s, RoutedEventArgs e) =>
            RunOp("解包", () => Native.rp_unpack(txtUnpackGame.Text.Trim(), txtUnpackOut.Text.Trim()));

        private void BtnBuild_Click(object s, RoutedEventArgs e) =>
            RunOp("重打包", () => Native.rp_build(txtBuildDir.Text.Trim(), txtBuildDir.Text.Trim() + ".rpa"));

        private void BtnMod_Click(object s, RoutedEventArgs e) =>
            RunOp("注入 Mod", () => Native.rp_mod(txtModTarget.Text.Trim(), txtModDir.Text.Trim()));

        private void BtnCredit_Click(object s, RoutedEventArgs e) =>
            RunOp("来源声明", () => Native.rp_credit_add(txtCreditGame.Text.Trim(), txtCreditOut.Text.Trim(), txtCreditName.Text.Trim()));

        private void BtnExport_Click(object s, RoutedEventArgs e)
        {
            int split = 0;
            int.TryParse(txtSplitMb.Text.Trim(), out split);
            RunOp("导出 zip", () => Native.rp_export(txtExportDir.Text.Trim(), txtExportOut.Text.Trim(), split));
        }

        private void BtnExportApk_Click(object s, RoutedEventArgs e) =>
            RunOp("导出 APK", () => Native.rp_export_apk(txtExportDir.Text.Trim(), txtApkMod.Text.Trim(), txtExportOut.Text.Trim() + ".apk", 1));

        private void BtnBranch_Click(object s, RoutedEventArgs e)
        {
            var fmt = (cmbBranchFmt.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "mermaid";
            RunOp("分支导图", () => Native.rp_branch(txtBranchDir.Text.Trim(), txtBranchOut.Text.Trim(), fmt));
        }

        // ---------------- 翻译 ----------------
        private void BtnTrExtract_Click(object s, RoutedEventArgs e) =>
            RunOp("抽取对话", () => Native.rp_translate_extract(txtTrDir.Text.Trim(), txtTrJson.Text.Trim()));

        private void BtnTrAi_Click(object s, RoutedEventArgs e)
        {
            var src = txtTrSrc.Text.Trim();
            var dst = txtTrOut.Text.Trim();
            if (src.Length == 0 || dst.Length == 0) { Log(2, "源/输出 JSON 必填"); return; }
            if (txtTrKey.Password.Length == 0) { Log(2, "API Key 必填"); return; }
            RunOp("AI 翻译", () => Native.rp_translate_ai(
                src, dst,
                txtTrFrom.Text.Trim(), txtTrTo.Text.Trim(),
                txtTrBase.Text.Trim(), txtTrKey.Password, txtTrModel.Text.Trim()));
        }

        private void BtnTrBuild_Click(object s, RoutedEventArgs e) =>
            RunOp("回写 Mod", () => Native.rp_translate_build(txtTrBuildJson.Text.Trim(), txtTrBuildOut.Text.Trim(), txtTrLang.Text.Trim()));

        // ---------------- 设置 ----------------
        private void ChkRound_Changed(object s, RoutedEventArgs e)
        {
            App.Settings.Round = chkRound.IsChecked == true;
            App.Current.Resources["CornerRadius"] = App.Settings.Round ? 8.0 : 0.0;
            App.Settings.Save();
        }
        private void ChkAnim_Changed(object s, RoutedEventArgs e)
        {
            App.Settings.Anim = chkAnim.IsChecked == true;
            App.Settings.Save();
        }
        private void ChkBlur_Changed(object s, RoutedEventArgs e)
        {
            App.Settings.Blur = chkBlur.IsChecked == true;
            ApplyBlur(App.Settings.Blur);
            App.Settings.Save();
        }
        private void CmbFont_Changed(object s, SelectionChangedEventArgs e)
        {
            if (cmbFont.SelectedItem == null) return;
            App.Settings.FontFamily = (cmbFont.SelectedItem as ComboBoxItem).Content.ToString();
            FontFamily = new FontFamily(App.Settings.FontFamily);
            App.Settings.Save();
        }
        private void CmbFontSize_Changed(object s, SelectionChangedEventArgs e)
        {
            if (cmbFontSize.SelectedItem == null) return;
            double v;
            if (double.TryParse((cmbFontSize.SelectedItem as ComboBoxItem).Content.ToString(), out v))
            {
                App.Settings.FontSize = v;
                FontSize = v;
                App.Settings.Save();
            }
        }
        private void BtnResetUi_Click(object s, RoutedEventArgs e)
        {
            App.Settings = new UiSettings(); App.Settings.Save();
            ApplySettings();
            Log(1, "界面设置已恢复默认");
        }
    }
}
