using System;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;

namespace RenPyPatcherGui
{
    /// <summary>
    /// 独立的「游戏操作」二级窗口：点主窗口列表项的「管理」打开，
    /// 自带解包 / 注入 / 来源声明 / 导出 / 分支导图 / 翻译流水线。
    /// 任务执行走 Ops.Run，日志统一在日志窗口显示。
    /// </summary>
    public partial class GameOpsWindow : Window
    {
        public GameOpsWindow(string gameName, string gamePath)
        {
            InitializeComponent();
            txtTitle.Text = "正在管理：" + gameName;
            // 预填参数：token 类填名称，目录类填路径
            txtUnpackGame.Text = gameName;
            txtCreditGame.Text = gameName;
            txtTrDir.Text = gamePath;
            txtBranchDir.Text = gamePath;
            txtExportDir.Text = gamePath;
            txtModTarget.Text = gamePath;
        }

        private void PickFolder(TextBox tb)
        {
            var d = new System.Windows.Forms.FolderBrowserDialog { Description = "选择文件夹" };
            if (d.ShowDialog() == System.Windows.Forms.DialogResult.OK) tb.Text = d.SelectedPath;
        }

        private void BtnBrowseUnpackOut_Click(object s, RoutedEventArgs e) => PickFolder(txtUnpackOut);
        private void BtnBrowseModDir_Click(object s, RoutedEventArgs e) => PickFolder(txtModDir);
        private void BtnBrowseExportDir_Click(object s, RoutedEventArgs e) => PickFolder(txtExportDir);
        private void BtnBrowseBranchDir_Click(object s, RoutedEventArgs e) => PickFolder(txtBranchDir);
        private void BtnBrowseTrDir_Click(object s, RoutedEventArgs e) => PickFolder(txtTrDir);

        private void BtnUnpack_Click(object s, RoutedEventArgs e) =>
            Ops.Run("解包", () => Native.rp_unpack(txtUnpackGame.Text.Trim(), txtUnpackOut.Text.Trim()));

        private void BtnBuild_Click(object s, RoutedEventArgs e) =>
            Ops.Run("重打包", () => Native.rp_build(txtBuildDir.Text.Trim(), txtBuildDir.Text.Trim() + ".rpa"));

        private void BtnMod_Click(object s, RoutedEventArgs e) =>
            Ops.Run("注入 Mod", () => Native.rp_mod(txtModTarget.Text.Trim(), txtModDir.Text.Trim()));

        private void BtnCredit_Click(object s, RoutedEventArgs e) =>
            Ops.Run("来源声明", () => Native.rp_credit_add(txtCreditGame.Text.Trim(), txtCreditOut.Text.Trim(), txtCreditName.Text.Trim()));

        private void BtnExport_Click(object s, RoutedEventArgs e)
        {
            int split = 0;
            int.TryParse(txtSplitMb.Text.Trim(), out split);
            Ops.Run("导出 zip", () => Native.rp_export(txtExportDir.Text.Trim(), txtExportOut.Text.Trim(), split));
        }

        private void BtnExportApk_Click(object s, RoutedEventArgs e) =>
            Ops.Run("导出 APK", () => Native.rp_export_apk(txtExportDir.Text.Trim(), txtApkMod.Text.Trim(), txtExportOut.Text.Trim() + ".apk", 1));

        private void BtnBranch_Click(object s, RoutedEventArgs e)
        {
            var fmt = (cmbBranchFmt.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "mermaid";
            Ops.Run("分支导图", () => Native.rp_branch(txtBranchDir.Text.Trim(), txtBranchOut.Text.Trim(), fmt));
        }

        private void BtnTrExtract_Click(object s, RoutedEventArgs e) =>
            Ops.Run("抽取对话", () => Native.rp_translate_extract(txtTrDir.Text.Trim(), txtTrJson.Text.Trim()));

        private void BtnTrAi_Click(object s, RoutedEventArgs e)
        {
            var src = txtTrSrc.Text.Trim();
            var dst = txtTrOut.Text.Trim();
            if (src.Length == 0 || dst.Length == 0) { LogHub.Add(2, "源/输出 JSON 必填"); return; }
            if (txtTrKey.Password.Length == 0) { LogHub.Add(2, "API Key 必填"); return; }
            Ops.Run("AI 翻译", () => Native.rp_translate_ai(
                src, dst,
                txtTrFrom.Text.Trim(), txtTrTo.Text.Trim(),
                txtTrBase.Text.Trim(), txtTrKey.Password, txtTrModel.Text.Trim()));
        }

        private void BtnTrBuild_Click(object s, RoutedEventArgs e) =>
            Ops.Run("回写 Mod", () => Native.rp_translate_build(txtTrBuildJson.Text.Trim(), txtTrBuildOut.Text.Trim(), txtTrLang.Text.Trim()));
    }
}
