using System;
using System.Collections.Generic;

namespace RenPyPatcherGui
{
    // 日志/进度共享中心：所有窗口（主窗口、日志窗口、操作窗口）共用。
    // C 库回调在主线程之外的线程触发，这里只做线程安全缓冲 + 广播事件，
    // 订阅者（如 LogWindow）自行用 Dispatcher 回到 UI 线程。
    public class LogEntry
    {
        public int Level { get; set; }
        public string Msg { get; set; }
    }

    public static class LogHub
    {
        private static readonly object _lock = new object();

        public static List<LogEntry> History { get; } = new List<LogEntry>();

        public static event Action<LogEntry> OnLog;
        public static event Action<int, string> OnProgress;

        public static void Add(int level, string msg)
        {
            if (string.IsNullOrEmpty(msg)) return;
            var e = new LogEntry { Level = level, Msg = msg };
            lock (_lock)
            {
                History.Add(e);
                if (History.Count > 2000) History.RemoveAt(0);
            }
            OnLog?.Invoke(e);
        }

        public static void SetProgress(int pct, string stage)
        {
            OnProgress?.Invoke(Math.Max(0, Math.Min(100, pct)), stage ?? "");
        }
    }
}
