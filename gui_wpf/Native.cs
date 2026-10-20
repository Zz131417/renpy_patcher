using System;
using System.Runtime.InteropServices;
using System.Text;

namespace RenPyPatcherGui
{
    /// <summary>
    /// P/Invoke 桥接到 patcher_core.dll（核心 C 库，UTF-8 字符串）。
    /// 所有阻塞操作应在后台线程调用，并通过回调把日志/进度抛回 UI 线程。
    /// </summary>
    internal static class Native
    {
        private const string Dll = "patcher_core";

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void LogCallback(int level, IntPtr msg, IntPtr user);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void ProgressCallback(int percent, IntPtr stage, IntPtr user);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int GameCallback(IntPtr name, IntPtr path, IntPtr note, IntPtr user);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern void rp_set_log_callback(LogCallback cb, IntPtr user);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern void rp_set_progress_callback(ProgressCallback cb, IntPtr user);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern void rp_set_log_level(int level);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr rp_error_message();

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPUTF8Str)]
        public static extern string rp_version_string();

        // 游戏管理
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern void rp_game_init([MarshalAs(UnmanagedType.LPUTF8Str)] string store_path);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_game_add([MarshalAs(UnmanagedType.LPUTF8Str)] string name,
                                             [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
                                             [MarshalAs(UnmanagedType.LPUTF8Str)] string note);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_game_remove([MarshalAs(UnmanagedType.LPUTF8Str)] string name);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_game_count();

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_game_list(GameCallback cb, IntPtr user);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr rp_game_resolve([MarshalAs(UnmanagedType.LPUTF8Str)] string token);

        // 资源操作
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_unpack([MarshalAs(UnmanagedType.LPUTF8Str)] string game_token,
                                          [MarshalAs(UnmanagedType.LPUTF8Str)] string out_dir);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_build([MarshalAs(UnmanagedType.LPUTF8Str)] string dir,
                                         [MarshalAs(UnmanagedType.LPUTF8Str)] string out_rpa);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_mod([MarshalAs(UnmanagedType.LPUTF8Str)] string dir,
                                       [MarshalAs(UnmanagedType.LPUTF8Str)] string mod_dir);

        // drop-in 模式：把 mod 装入已解包游戏 game/ 下，生成 game/mod/ 与 game/dll/ + bootstrap
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_mod_dropin([MarshalAs(UnmanagedType.LPUTF8Str)] string game_dir,
                                              [MarshalAs(UnmanagedType.LPUTF8Str)] string mod_src,
                                              [MarshalAs(UnmanagedType.LPUTF8Str)] string dll_dir);

        // 进程级 DLL 注入（Windows）
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_inject_dll(int pid, [MarshalAs(UnmanagedType.LPUTF8Str)] string dll_path);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_inject_dll_launch([MarshalAs(UnmanagedType.LPUTF8Str)] string exe_path,
                                                     [MarshalAs(UnmanagedType.LPUTF8Str)] string dll_path,
                                                     int delay_ms);

        // 直接注入（烘焙）：把 DLL 放游戏包并重打包
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_inject_dll_bake([MarshalAs(UnmanagedType.LPUTF8Str)] string game_rpa,
                                                    [MarshalAs(UnmanagedType.LPUTF8Str)] string dll_path,
                                                    [MarshalAs(UnmanagedType.LPUTF8Str)] string out_rpa);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_credit_add([MarshalAs(UnmanagedType.LPUTF8Str)] string game_token,
                                              [MarshalAs(UnmanagedType.LPUTF8Str)] string out_rpa,
                                              [MarshalAs(UnmanagedType.LPUTF8Str)] string game_name);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr rp_credit_text([MarshalAs(UnmanagedType.LPUTF8Str)] string game_name);

        // 导出
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_export([MarshalAs(UnmanagedType.LPUTF8Str)] string dir,
                                          [MarshalAs(UnmanagedType.LPUTF8Str)] string out_path,
                                          int split_mb);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_export_apk([MarshalAs(UnmanagedType.LPUTF8Str)] string apk,
                                             [MarshalAs(UnmanagedType.LPUTF8Str)] string mod_dir,
                                             [MarshalAs(UnmanagedType.LPUTF8Str)] string out_apk,
                                             int strip_sig);

        // 分支导图
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_branch([MarshalAs(UnmanagedType.LPUTF8Str)] string dir,
                                          [MarshalAs(UnmanagedType.LPUTF8Str)] string out_path,
                                          [MarshalAs(UnmanagedType.LPUTF8Str)] string fmt);

        // 翻译
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_translate_extract([MarshalAs(UnmanagedType.LPUTF8Str)] string dir,
                                                     [MarshalAs(UnmanagedType.LPUTF8Str)] string out_json);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_translate_ai([MarshalAs(UnmanagedType.LPUTF8Str)] string src_json,
                                                [MarshalAs(UnmanagedType.LPUTF8Str)] string out_json,
                                                [MarshalAs(UnmanagedType.LPUTF8Str)] string from,
                                                [MarshalAs(UnmanagedType.LPUTF8Str)] string to,
                                                [MarshalAs(UnmanagedType.LPUTF8Str)] string base_url,
                                                [MarshalAs(UnmanagedType.LPUTF8Str)] string api_key,
                                                [MarshalAs(UnmanagedType.LPUTF8Str)] string model);

        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
        public static extern int rp_translate_build([MarshalAs(UnmanagedType.LPUTF8Str)] string json,
                                                   [MarshalAs(UnmanagedType.LPUTF8Str)] string out_dir,
                                                   [MarshalAs(UnmanagedType.LPUTF8Str)] string lang);

        // ---- Windows DWM 背景模糊 ----
        [StructLayout(LayoutKind.Sequential)]
        public struct DWM_BLURBEHIND
        {
            public uint dwFlags;
            [MarshalAs(UnmanagedType.Bool)] public bool fEnable;
            public IntPtr hRgnBlur;
            [MarshalAs(UnmanagedType.Bool)] public bool fTransitionOnMaximized;
        }

        [DllImport("dwmapi")]
        public static extern int DwmEnableBlurBehindWindow(IntPtr hwnd, ref DWM_BLURBEHIND bb);

        // ---- 便捷封装 ----
        public static string PtrToUtf8(IntPtr p)
        {
            if (p == IntPtr.Zero) return string.Empty;
            return Marshal.PtrToStringUTF8(p) ?? string.Empty;
        }
    }
}
