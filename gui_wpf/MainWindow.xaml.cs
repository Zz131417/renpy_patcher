using System.Windows.Media;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
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

        // 游戏列表数据源
        private ObservableCollection<GameEntry> _games = new ObservableCollection<GameEntry>();
        private List<GameEntry> _pendingGames = new List<GameEntry>();

        // 数据目录（位于可执行文件目录下，便于便携）：game/ 存游戏，settings/ 存登记表与界面设置
        private string _baseDir, _gameDir, _settingsDir;

        // 日志窗口单例
        private LogWindow _logWin;

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

            ApplySettings();
            RefreshGames();
            LogHub.Add(1, "GUI 已就绪");
        }

        // ---------------- 设置应用（本窗口外观） ----------------
        private void ApplySettings()
        {
            var s = App.Settings;
            FontFamily = new FontFamily(s.FontFamily);
            FontSize = s.FontSize;
            App.Current.Resources["CornerRadius"] = s.Round ? 8.0 : 0.0;
            ApplyDwmBlur(this, s.Blur);
        }

        private void ApplyDwmBlur(Window w, bool on)
        {
            try
            {
                var bb = new Native.DWM_BLURBEHIND
                {
                    dwFlags = 0x1 /*DWM_BB_ENABLE*/,
                    fEnable = on,
                    hRgnBlur = IntPtr.Zero,
                    fTransitionOnMaximized = false
                };
                Native.DwmEnableBlurBehindWindow(new System.Windows.Interop.WindowInteropHelper(w).Handle, ref bb);
            }
            catch { }
        }

        // ---------------- 回调（统一转发到 LogHub） ----------------
        private void LogCallbackImpl(int level, IntPtr msg, IntPtr user)
        {
            LogHub.Add(level, Native.PtrToUtf8(msg));
        }

        private void ProgressCallbackImpl(int percent, IntPtr stage, IntPtr user)
        {
            LogHub.SetProgress(percent, Native.PtrToUtf8(stage));
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

        // ---------------- 二级窗口入口 ----------------
        private void BtnSettings_Click(object s, RoutedEventArgs e)
        {
            var w = new SettingsWindow { Owner = this };
            w.Show();
        }

        private void BtnLog_Click(object s, RoutedEventArgs e)
        {
            if (_logWin == null || !_logWin.IsLoaded)
            {
                _logWin = new LogWindow { Owner = this };
                _logWin.Closed += (_, __) => _logWin = null;
                _logWin.Show();
            }
            else
            {
                _logWin.Activate();
            }
        }

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
            if (name.Length == 0 || path.Length == 0) { LogHub.Add(2, "名称与路径均必填"); return; }
            int rc = Native.rp_game_add(name, path, txtAliasNote.Text.Trim());
            LogHub.Add(rc == 0 ? 1 : 3, rc == 0 ? $"已登记：{name}" : "登记失败");
            RefreshGames();
        }

        private void BtnAliasRemove_Click(object s, RoutedEventArgs e)
        {
            var sel = lstGames.SelectedItem as GameEntry;
            if (sel == null) return;
            int rc = Native.rp_game_remove(sel.Name);
            LogHub.Add(rc == 0 ? 1 : 3, rc == 0 ? $"已删除：{sel.Name}" : "删除失败");
            RefreshGames();
        }

        // ---------------- 右下角浮动「添加游戏」：点击后展开内联表单并滚到顶部 ----------------
        private void BtnAddFab_Click(object s, RoutedEventArgs e)
        {
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
            if (name.Length == 0) { LogHub.Add(2, "无法取得游戏名"); return; }
            string dest = System.IO.Path.Combine(_gameDir, name);
            string captured = name;
            Ops.Run("从压缩包导入", () =>
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
                if (name.Length == 0) { LogHub.Add(2, "无法取得文件夹名"); return; }
                string captured = name, dir = path;
                Ops.Run("拖入文件夹（添加游戏）", () => Native.rp_game_add(captured, dir, "拖入文件夹"));
            }
            else if (File.Exists(path) && string.Equals(System.IO.Path.GetExtension(path), ".zip", StringComparison.OrdinalIgnoreCase))
            {
                // 拖入 zip：解压进 game/<名称>/
                string name = System.IO.Path.GetFileNameWithoutExtension(path);
                string dest = System.IO.Path.Combine(_gameDir, name);
                string captured = name, src = path;
                Ops.Run("拖入压缩包（导入）", () =>
                {
                    System.IO.Directory.CreateDirectory(dest);
                    ZipFile.ExtractToDirectory(src, dest, true);
                    return Native.rp_game_add(captured, dest, "拖入压缩包导入");
                });
            }
            else
            {
                LogHub.Add(2, "不支持的拖入类型：" + path);
            }
        }

        // ---------------- 列表项：管理（打开二级操作窗口）/ 启动 ----------------
        private void BtnGameManage_Click(object s, RoutedEventArgs e)
        {
            var name = (s as Button)?.Tag as string;
            if (string.IsNullOrEmpty(name)) return;
            string path = Native.PtrToUtf8(Native.rp_game_resolve(name));
            if (string.IsNullOrEmpty(path)) { LogHub.Add(2, "找不到游戏路径：" + name); return; }
            var w = new GameOpsWindow(name, path) { Owner = this };
            w.Show();
            LogHub.Add(1, $"已打开「{name}」的操作窗口");
        }

        private void BtnGameLaunch_Click(object s, RoutedEventArgs e)
        {
            var name = (s as Button)?.Tag as string;
            if (string.IsNullOrEmpty(name)) return;
            string raw = Native.PtrToUtf8(Native.rp_game_resolve(name));
            if (string.IsNullOrEmpty(raw)) { LogHub.Add(2, "找不到游戏路径"); return; }
            try
            {
                if (Directory.Exists(raw))
                {
                    var exe = Directory.GetFiles(raw, "*.exe", SearchOption.TopDirectoryOnly)
                        .FirstOrDefault(x => !Path.GetFileName(x).ToLower().Contains("uninstall"));
                    if (exe != null) { Process.Start(exe); LogHub.Add(1, "启动：" + exe); }
                    else { Process.Start("explorer.exe", raw); LogHub.Add(1, "未找到 exe，已打开目录"); }
                }
                else if (File.Exists(raw))
                {
                    Process.Start(raw); LogHub.Add(1, "启动：" + raw);
                }
                else { LogHub.Add(2, "路径不存在：" + raw); }
            }
            catch (Exception ex) { LogHub.Add(3, "启动失败：" + ex.Message); }
        }

        // ---------------- 文件/文件夹选择 ----------------
        private void PickFolder(TextBox tb)
        {
            var d = new System.Windows.Forms.FolderBrowserDialog { Description = "选择文件夹" };
            if (d.ShowDialog() == System.Windows.Forms.DialogResult.OK) tb.Text = d.SelectedPath;
        }
        private void BtnBrowseAlias_Click(object s, RoutedEventArgs e) => PickFolder(txtAliasPath);
    }
}
