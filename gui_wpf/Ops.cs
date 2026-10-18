using System;
using System.Threading.Tasks;

namespace RenPyPatcherGui
{
    // 后台任务封装：全局串行（同一时间只跑一个），日志/进度全部走 LogHub，
    // 因此任何窗口（主窗口、游戏操作窗口）都能发起任务并在日志窗口看到。
    public static class Ops
    {
        private static bool _busy = false;
        private const int RP_ERR_UNKNOWN = -1;

        public static async void Run(string name, Func<int> work)
        {
            if (_busy) { LogHub.Add(2, "上一项任务进行中，请稍候"); return; }
            _busy = true;
            LogHub.SetProgress(0, name + " 进行中…");
            int rc = RP_ERR_UNKNOWN;
            await Task.Run(() => { rc = work(); });
            if (rc == 0)
                LogHub.Add(1, name + "：完成 ✅");
            else
                LogHub.Add(3, name + "：失败 ❌ " + Native.PtrToUtf8(Native.rp_error_message()));
            LogHub.SetProgress(100, name + (rc == 0 ? " 完成" : " 失败"));
            _busy = false;
        }
    }
}
