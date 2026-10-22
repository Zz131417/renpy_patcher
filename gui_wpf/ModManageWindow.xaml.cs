using System;
using System.Collections.Generic;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;

namespace RenPyPatcherGui
{
    /// <summary>
    /// 三级页面「Mod 管理」：从「游戏操作」二级窗口打开。
    /// 内含「注入 Mod 引擎」（Drop-in 自动读取 / 运行时热注入）与已安装 Mod 的启用/禁用管理。
    /// </summary>
    public class ModItem
    {
        public string Name { get; set; }
        public string Path { get; set; }
        public bool Enabled { get; set; }
    }

    public partial class ModManageWindow : Window
    {
        private readonly string _gameName;
        private readonly string _gamePath;

        public ModManageWindow(string gameName, string gamePath)
        {
            InitializeComponent();
            _gameName = gameName;
            _gamePath = gamePath;
            txtTitle.Text = "Mod 管理：" + gameName;
            txtDiGame.Text = gamePath;
            string md = ResolveModDir();
            if (md != null) txtDiMod.Text = md;
            ScanMods();
        }

        // ---------------- 注入引擎 ----------------
        private void EngineMode_Changed(object s, RoutedEventArgs e)
        {
            bool dropin = rbDropin.IsChecked == true;
            gridDropin.Visibility = dropin ? Visibility.Visible : Visibility.Collapsed;
            gridHot.Visibility = dropin ? Visibility.Collapsed : Visibility.Visible;
        }

        private void PickFolder(TextBox tb)
        {
            var d = new System.Windows.Forms.FolderBrowserDialog { Description = "选择文件夹" };
            if (d.ShowDialog() == System.Windows.Forms.DialogResult.OK) tb.Text = d.SelectedPath;
        }
        private void PickFile(TextBox tb, string filter)
        {
            var d = new OpenFileDialog { Filter = filter, CheckFileExists = true };
            if (d.ShowDialog() == true) tb.Text = d.FileName;
        }
        private void BtnDiGame_Click(object s, RoutedEventArgs e) => PickFolder(txtDiGame);
        private void BtnDiMod_Click(object s, RoutedEventArgs e) => PickFolder(txtDiMod);
        private void BtnDiDll_Click(object s, RoutedEventArgs e) => PickFolder(txtDiDll);
        private void BtnHotDll_Click(object s, RoutedEventArgs e) =>
            PickFile(txtHotDll, "动态库 (*.dll)|*.dll|所有文件 (*.*)|*.*");

        private void BtnDiInstall_Click(object s, RoutedEventArgs e) =>
            Ops.Run("安装 Drop-in", () => Native.rp_mod_dropin(txtDiGame.Text.Trim(), txtDiMod.Text.Trim(), txtDiDll.Text.Trim()));

        private void BtnHotInject_Click(object s, RoutedEventArgs e)
        {
            var dll = txtHotDll.Text.Trim();
            if (dll.Length == 0) { LogHub.Add(2, "请选择要注入的 DLL"); return; }
            if (!int.TryParse(txtHotPid.Text.Trim(), out int pid) || pid <= 0) { LogHub.Add(2, "PID 非法"); return; }
            Ops.Run("进程注入 DLL", () => Native.rp_inject_dll(pid, dll));
        }

        // ---------------- 已安装 Mod 列表 ----------------
        private string ResolveModDir()
        {
            string[] cands = {
                Path.Combine(_gamePath, "game", "mod"),
                Path.Combine(_gamePath, "mod")
            };
            foreach (var c in cands) if (Directory.Exists(c)) return c;
            return null;
        }

        private void ScanMods()
        {
            string dir = ResolveModDir();
            var items = new List<ModItem>();
            if (dir == null)
            {
                txtModHint.Text = "未检测到 game/mod/ 目录。先在上方的「Drop-in」面板安装一次 Mod，即可在此管理启用状态。";
            }
            else
            {
                txtModHint.Text = "勾选/取消「启用」可临时关闭某个 mod 文件（重命名为 .disabled，游戏将忽略该文件）。";
                foreach (var f in Directory.GetFiles(dir, "*", SearchOption.AllDirectories))
                {
                    string name = Path.GetFileName(f);
                    bool disabled = name.EndsWith(".disabled", StringComparison.OrdinalIgnoreCase);
                    string disp = disabled
                        ? name.Substring(0, name.Length - ".disabled".Length) + " （已禁用）"
                        : name;
                    items.Add(new ModItem { Name = disp, Path = f, Enabled = !disabled });
                }
            }
            lstMods.ItemsSource = items;
        }

        private void ModEnabled_Changed(object s, RoutedEventArgs e)
        {
            if (!(s is CheckBox cb) || !(cb.Tag is ModItem it)) return;
            try
            {
                string newPath;
                if (it.Enabled) // 想启用
                    newPath = it.Path.EndsWith(".disabled", StringComparison.OrdinalIgnoreCase)
                        ? it.Path.Substring(0, it.Path.Length - ".disabled".Length) : null;
                else // 想禁用
                    newPath = it.Path.EndsWith(".disabled", StringComparison.OrdinalIgnoreCase)
                        ? null : it.Path + ".disabled";
                if (newPath != null && newPath != it.Path)
                    File.Move(it.Path, newPath);
            }
            catch (Exception ex) { LogHub.Add(3, "切换 mod 启用状态失败：" + ex.Message); }
            ScanMods();
        }

        private void BtnRefreshMods_Click(object s, RoutedEventArgs e) => ScanMods();
        private void BtnOpenModDir_Click(object s, RoutedEventArgs e)
        {
            string dir = ResolveModDir();
            if (dir == null) { LogHub.Add(2, "未检测到 game/mod/ 目录"); return; }
            try
            {
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo { FileName = dir, UseShellExecute = true });
            }
            catch (Exception ex) { LogHub.Add(3, ex.Message); }
        }
    }
}
