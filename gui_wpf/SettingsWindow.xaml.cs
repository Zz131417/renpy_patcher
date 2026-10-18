using System.Windows.Controls;
using System;
using System.Windows;
using System.Windows.Media;
using Microsoft.Win32;

namespace RenPyPatcherGui
{
    public partial class SettingsWindow : Window
    {
        public SettingsWindow()
        {
            InitializeComponent();
            var s = App.Settings;
            chkRound.IsChecked = s.Round;
            chkAnim.IsChecked = s.Anim;
            chkBlur.IsChecked = s.Blur;
            cmbFont.Text = s.FontFamily;
            cmbFontSize.Text = s.FontSize.ToString();
        }

        // 把外观应用到主窗口（实时）
        private void ApplyToMainWindow()
        {
            var mw = Application.Current.MainWindow;
            if (mw == null) return;
            var s = App.Settings;
            mw.FontFamily = new FontFamily(s.FontFamily);
            mw.FontSize = s.FontSize;
            App.Current.Resources["CornerRadius"] = s.Round ? 8.0 : 0.0;
            ApplyDwmBlur(mw, s.Blur);
        }

        private static void ApplyDwmBlur(Window w, bool on)
        {
            try
            {
                var bb = new Native.DWM_BLURBEHIND
                {
                    dwFlags = 0x1,
                    fEnable = on,
                    hRgnBlur = IntPtr.Zero,
                    fTransitionOnMaximized = false
                };
                Native.DwmEnableBlurBehindWindow(new System.Windows.Interop.WindowInteropHelper(w).Handle, ref bb);
            }
            catch { }
        }

        private void ChkRound_Changed(object s, RoutedEventArgs e)
        {
            App.Settings.Round = chkRound.IsChecked == true;
            App.Settings.Save();
            ApplyToMainWindow();
        }
        private void ChkAnim_Changed(object s, RoutedEventArgs e)
        {
            App.Settings.Anim = chkAnim.IsChecked == true;
            App.Settings.Save();
        }
        private void ChkBlur_Changed(object s, RoutedEventArgs e)
        {
            App.Settings.Blur = chkBlur.IsChecked == true;
            App.Settings.Save();
            ApplyToMainWindow();
        }
        private void CmbFont_Changed(object s, SelectionChangedEventArgs e)
        {
            if (cmbFont.SelectedItem == null) return;
            App.Settings.FontFamily = (cmbFont.SelectedItem as ComboBoxItem).Content.ToString();
            App.Settings.Save();
            ApplyToMainWindow();
        }
        private void CmbFontSize_Changed(object s, SelectionChangedEventArgs e)
        {
            if (cmbFontSize.SelectedItem == null) return;
            double v;
            if (double.TryParse((cmbFontSize.SelectedItem as ComboBoxItem).Content.ToString(), out v))
            {
                App.Settings.FontSize = v;
                App.Settings.Save();
                ApplyToMainWindow();
            }
        }
        private void BtnReset_Click(object s, RoutedEventArgs e)
        {
            App.Settings = new UiSettings(); App.Settings.Save();
            chkRound.IsChecked = App.Settings.Round;
            chkAnim.IsChecked = App.Settings.Anim;
            chkBlur.IsChecked = App.Settings.Blur;
            cmbFont.Text = App.Settings.FontFamily;
            cmbFontSize.Text = App.Settings.FontSize.ToString();
            ApplyToMainWindow();
            LogHub.Add(1, "界面设置已恢复默认");
        }
        private void BtnClose_Click(object s, RoutedEventArgs e) => Close();
    }
}
