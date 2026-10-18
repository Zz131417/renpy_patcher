using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using Microsoft.Win32;

namespace RenPyPatcherGui
{
    public partial class MainWindow : Window
    {
        // 保存委托引用，防止被 GC 回收
        private Native.LogCallback _logCb;
        private Native.ProgressCallback _progCb;
        private Native.GameCallback _gameCb;
        private bool _busy = false;
        private int _currentPage = 0;

        public MainWindow()
        {
            InitializeComponent();
            Loaded += MainWindow_Loaded;
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            // 初始化核心 + 回调
            Native.rp_game_init(null);
            _logCb = LogCallbackImpl;
            _progCb = ProgressCallbackImpl;
            Native.rp_set_log_callback(_logCb, IntPtr.Zero);
            Native.rp_set_progress_callback(_progCb, IntPtr.Zero);
            Native.rp_set_log_level(0);

            _gameCb = GameCallbackImpl;

            txtVer.Text = "版本 " + Native.rp_version_string();

            ApplySettings();
            RefreshGames();
            SwitchPage(0, false);
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

        // ---------------- 导航 ----------------
        private void BtnNavGame_Click(object s, RoutedEventArgs e) => SwitchPage(0);
        private void BtnNavHome_Click(object s, RoutedEventArgs e) => SwitchPage(1);
        private void BtnNavSettings_Click(object s, RoutedEventArgs e) => SwitchPage(2);

        private void SwitchPage(int idx, bool animate = true)
        {
            _currentPage = idx;
            pageGame.Visibility = idx == 0 ? Visibility.Visible : Visibility.Collapsed;
            pageHome.Visibility = idx == 1 ? Visibility.Visible : Visibility.Collapsed;
            pageSettings.Visibility = idx == 2 ? Visibility.Visible : Visibility.Collapsed;

            btnNavGame.FontWeight = idx == 0 ? FontWeights.Bold : FontWeights.SemiBold;
            btnNavHome.FontWeight = idx == 1 ? FontWeights.Bold : FontWeights.SemiBold;
            btnNavSettings.FontWeight = idx == 2 ? FontWeights.Bold : FontWeights.SemiBold;
            btnNavGame.Foreground = idx == 0 ? new SolidColorBrush(Color.FromRgb(0x25, 0x63, 0xEB)) : new SolidColorBrush(Color.FromRgb(0x33, 0x41, 0x55));
            btnNavHome.Foreground = idx == 1 ? new SolidColorBrush(Color.FromRgb(0x25, 0x63, 0xEB)) : new SolidColorBrush(Color.FromRgb(0x33, 0x41, 0x55));
            btnNavSettings.Foreground = idx == 2 ? new SolidColorBrush(Color.FromRgb(0x25, 0x63, 0xEB)) : new SolidColorBrush(Color.FromRgb(0x33, 0x41, 0x55));

            if (animate && App.Settings.Anim)
            {
                PageTx.Y = 14;
                var anim = new DoubleAnimation(0, TimeSpan.FromMilliseconds(220))
                { EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut } };
                PageTx.BeginAnimation(TranslateTransform.YProperty, anim);
            }
            else
            {
                PageTx.Y = 0;
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
            // 在后台线程调用，收集到列表后统一回 UI 线程刷新
            _pendingGames.Add($"{Native.PtrToUtf8(name)}  |  {Native.PtrToUtf8(path)}  |  {Native.PtrToUtf8(note)}");
            return 0;
        }
        private List<string> _pendingGames = new List<string>();

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
            lstGames.Items.Clear();
            foreach (var g in _pendingGames) lstGames.Items.Add(g);
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
            if (lstGames.SelectedItem == null) return;
            var line = lstGames.SelectedItem.ToString();
            var name = line.Split('|')[0].Trim();
            int rc = Native.rp_game_remove(name);
            Log(rc == 0 ? 1 : 3, rc == 0 ? $"已删除：{name}" : "删除失败");
            RefreshGames();
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
