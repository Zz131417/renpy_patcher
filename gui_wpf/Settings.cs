using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace RenPyPatcherGui
{
    /// <summary>
    /// UI 设置（界面外观，持久化到本地 gui.json）。与核心解包无关，纯前端偏好。
    /// </summary>
    public class UiSettings
    {
        public bool Round { get; set; } = true;
        public bool Anim { get; set; } = true;
        public bool Blur { get; set; } = false;
        public string FontFamily { get; set; } = "Microsoft YaHei UI";
        public double FontSize { get; set; } = 12;

        private static readonly string Path =
            System.IO.Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "RenPyPatcher", "gui.json");

        public static UiSettings Load()
        {
            try
            {
                if (File.Exists(Path))
                {
                    var s = JsonSerializer.Deserialize<UiSettings>(File.ReadAllText(Path));
                    if (s != null) return s;
                }
            }
            catch { }
            return new UiSettings();
        }

        public void Save()
        {
            try
            {
                var dir = System.IO.Path.GetDirectoryName(Path);
                if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
                File.WriteAllText(Path, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
            }
            catch { }
        }
    }
}
