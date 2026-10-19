using System;
using System.IO;
using System.Threading.Tasks;
using System.Windows;
using Microsoft.Win32;

namespace RenPyPatcherGui
{
    // 独立的 DLL 注入窗口：选 DLL + 选目标（运行中进程 PID 或 启动 exe 并注入）。
    // 底层走 patcher_core.dll 的 rp_inject_dll / rp_inject_dll_launch（经典 LoadLibrary 远程线程）。
    public partial class InjectWindow : Window
    {
        public InjectWindow() { InitializeComponent(); }

        private void PickFile(TextBox tb, string filter)
        {
            var d = new OpenFileDialog { Filter = filter, CheckFileExists = true };
            if (d.ShowDialog() == true) tb.Text = d.FileName;
        }
        private void BtnBrowseDll_Click(object s, RoutedEventArgs e) =>
            PickFile(txtDll, "动态库 (*.dll)|*.dll|所有文件 (*.*)|*.*");
        private void BtnBrowseExe_Click(object s, RoutedEventArgs e) =>
            PickFile(txtExe, "可执行文件 (*.exe)|*.exe|所有文件 (*.*)|*.*");

        private void BtnInject_Click(object s, RoutedEventArgs e)
        {
            var dll = txtDll.Text.Trim();
            if (dll.Length == 0 || !File.Exists(dll)) { txtStatus.Text = "❌ 请选择有效的 DLL 文件"; return; }

            if (rbPid.IsChecked == true)
            {
                if (!int.TryParse(txtPid.Text.Trim(), out int pid) || pid <= 0) { txtStatus.Text = "❌ PID 非法"; return; }
                txtStatus.Text = $"注入中 → 进程 {pid} …";
                Task.Run(() =>
                {
                    int rc = Native.rp_inject_dll(pid, dll);
                    string msg = rc == 0
                        ? $"✅ 已注入到进程 {pid}"
                        : $"❌ 注入失败：{Native.PtrToUtf8(Native.rp_error_message())}";
                    Dispatcher.Invoke(() => { txtStatus.Text = msg; LogHub.Add(rc == 0 ? 1 : 3, msg); });
                });
            }
            else
            {
                var exe = txtExe.Text.Trim();
                if (exe.Length == 0 || !File.Exists(exe)) { txtStatus.Text = "❌ 启动程序无效"; return; }
                int.TryParse(txtDelay.Text.Trim(), out int delay);
                txtStatus.Text = $"启动并注入中 → {exe} …";
                Task.Run(() =>
                {
                    int rc = Native.rp_inject_dll_launch(exe, dll, delay);
                    string msg = rc == 0
                        ? $"✅ 已启动并注入：{exe}"
                        : $"❌ 注入失败：{Native.PtrToUtf8(Native.rp_error_message())}";
                    Dispatcher.Invoke(() => { txtStatus.Text = msg; LogHub.Add(rc == 0 ? 1 : 3, msg); });
                });
            }
        }

        private void BtnClose_Click(object s, RoutedEventArgs e) => Close();
    }
}
