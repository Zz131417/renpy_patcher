using System;
using System.Windows;

namespace RenPyPatcherGui
{
    /// <summary>
    /// 独立的「日志」二级窗口：显示 LogHub 的历史与实时日志 + 进度。
    /// 订阅 LogHub 事件，回到 UI 线程更新。
    /// </summary>
    public partial class LogWindow : Window
    {
        public LogWindow()
        {
            InitializeComponent();
            Loaded += (s, e) =>
            {
                foreach (var h in LogHub.History) AppendLog(h);
                LogHub.OnLog += LogHub_OnLog;
                LogHub.OnProgress += LogHub_OnProgress;
            };
            Closed += (s, e) =>
            {
                LogHub.OnLog -= LogHub_OnLog;
                LogHub.OnProgress -= LogHub_OnProgress;
            };
        }

        private void LogHub_OnLog(LogEntry e) => Dispatcher.Invoke(() => AppendLog(e));

        private void LogHub_OnProgress(int pct, string stage) => Dispatcher.Invoke(() =>
        {
            pbProgress.Value = pct;
            if (!string.IsNullOrEmpty(stage)) txtStatus.Text = stage;
        });

        private void AppendLog(LogEntry e)
        {
            lstLog.Items.Add(e.Msg);
            if (lstLog.Items.Count > 500) lstLog.Items.RemoveAt(0);
            lstLog.ScrollIntoView(lstLog.Items[lstLog.Items.Count - 1]);
        }
    }
}
