using System;
using System.Windows;

namespace RenPyPatcherGui
{
    public partial class App : Application
    {
        public static UiSettings Settings { get; set; }

        protected override void OnStartup(StartupEventArgs e)
        {
            Settings = UiSettings.Load();
            base.OnStartup(e);
        }
    }
}
