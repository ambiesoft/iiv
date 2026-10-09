using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Imaging;

namespace iiv_view;

public partial class MainWindow : Window
{
    private Point _dragStart;
    private double _startX;
    private double _startY;
    private double _scale = 1.0;

    private record WindowSettings
    {
        public double Width { get; init; }
        public double Height { get; init; }
        public double Left { get; init; }
        public double Top { get; init; }
        public bool IsMaximized { get; init; }
    }

    private static string SettingsFilePath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "iiv_view", "window.json");

    public MainWindow()
    {
        InitializeComponent();

        // restore window geometry first
        LoadWindowSizeFromSettings();

        // ensure title shows initial scale
        Title = "iiv_view";
        UpdateTitleScale();

        Loaded += (_, _) =>
        {
            LoadImage();

            // Run after layout / rendering so the window can actually be shown,
            // then briefly set Topmost to bring it to foreground and restore.
            Dispatcher.BeginInvoke(new Action(() =>
            {
                try
                {
                    Topmost = true;
                    Activate();
                    Topmost = false;
                }
                catch
                {
                    // ignore any exceptions when trying to activate
                }
            }), System.Windows.Threading.DispatcherPriority.ApplicationIdle);
        };

        Closing += (_, _) => SaveWindowSizeToSettings();

        // Mouse handlers for drag/zoom
        MouseLeftButtonDown += OnMouseLeftButtonDown;
        MouseLeftButtonUp += OnMouseLeftButtonUp;
        MouseMove += OnMouseMove;
        MouseWheel += OnMouseWheel;
        MouseRightButtonUp += OnMouseRightButtonUp;
        KeyDown += OnKeyDown;

        // Create a simple context menu for right-click
        var ctx = new ContextMenu();

        var miClose = new MenuItem { Header = "Close" };
        miClose.Click += (_, _) => Close();

        var miCloseAll = new MenuItem { Header = "Close All" };
        miCloseAll.Click += (_, _) => CloseAllWindows();

        ctx.Items.Add(miClose);
        ctx.Items.Add(new Separator());
        ctx.Items.Add(miCloseAll);

        // Attach to the main window (so right-click anywhere shows it).
        ContextMenu = ctx;
    }

    private void OnMouseRightButtonUp(object sender, MouseButtonEventArgs e)
    {
        // Show the context menu at the mouse position if not shown automatically.
        // If ContextMenu is attached to the Window, WPF often opens it automatically,
        // but showing it explicitly is harmless.
        if (ContextMenu != null)
        {
            ContextMenu.PlacementTarget = this;
            ContextMenu.IsOpen = true;
        }
    }

    private void CloseAllWindows()
    {
        try
        {
            var currentPid = System.Diagnostics.Process.GetCurrentProcess().Id;
            var procs = System.Diagnostics.Process.GetProcessesByName("iiv_view");

            foreach (var p in procs)
            {
                try
                {
                    if (p.Id == currentPid)
                        continue; // close self at the end

                    // Try to politely close the GUI (sends WM_CLOSE to main window)
                    if (p.CloseMainWindow())
                    {
                        // Wait briefly for graceful exit
                        if (!p.WaitForExit(1000))
                        {
                            try { p.Kill(); } catch { /* ignore */ }
                        }
                    }
                    else
                    {
                        // No main window or didn't accept close - force terminate
                        try { p.Kill(); } catch { /* ignore */ }
                    }
                }
                catch
                {
                    // ignore per-process failures
                }
                finally
                {
                    p.Dispose();
                }
            }

            // Close other windows in this process (collect first)
            var others = new List<Window>();
            foreach (Window w in Application.Current.Windows)
            {
                if (w != this)
                    others.Add(w);
            }

            foreach (var w in others)
            {
                try { w.Close(); } catch { /* ignore */ }
            }

            // Finally close this window
            Close();
        }
        catch
        {
            // ensure we at least close ourselves on unexpected error
            try { Close(); } catch { /* ignore */ }
        }
    }

    private void LoadWindowSizeFromSettings()
    {
        try
        {
            if (!File.Exists(SettingsFilePath))
                return;

            var json = File.ReadAllText(SettingsFilePath);
            var settings = JsonSerializer.Deserialize<WindowSettings>(json);
            if (settings == null)
                return;

            // Apply as manual startup location so Left/Top are respected.
            WindowStartupLocation = WindowStartupLocation.Manual;

            // Validate values
            if (double.IsFinite(settings.Width) && settings.Width > 0 &&
                double.IsFinite(settings.Height) && settings.Height > 0)
            {
                Width = settings.Width;
                Height = settings.Height;
            }

            if (double.IsFinite(settings.Left) && double.IsFinite(settings.Top))
            {
                Left = settings.Left;
                Top = settings.Top;
            }

            // Ensure window is on-screen (simple clamp to work area)
            var wa = SystemParameters.WorkArea;
            if (Left + Width < wa.Left || Top + Height < wa.Top || Left > wa.Right || Top > wa.Bottom)
            {
                Left = Math.Max(wa.Left, wa.Left + (wa.Width - Width) / 2);
                Top = Math.Max(wa.Top, wa.Top + (wa.Height - Height) / 2);
            }

            if (settings.IsMaximized)
            {
                // Defer maximizing until after layout if needed - setting here is acceptable.
                WindowState = WindowState.Maximized;
            }
        }
        catch
        {
            // ignore failures to avoid blocking startup
        }
    }

    private void SaveWindowSizeToSettings()
    {
        try
        {
            var dir = Path.GetDirectoryName(SettingsFilePath);
            if (!string.IsNullOrEmpty(dir))
                Directory.CreateDirectory(dir);

            // Use RestoreBounds to get the unmaximized bounds (so we can restore correct size later).
            var bounds = this.RestoreBounds;

            var settings = new WindowSettings
            {
                IsMaximized = WindowState == WindowState.Maximized,
                Width = bounds.Width,
                Height = bounds.Height,
                Left = bounds.Left,
                Top = bounds.Top
            };

            var json = JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(SettingsFilePath, json);
        }
        catch
        {
            // ignore IO errors silently
        }
    }

    private void LoadImage()
    {
        string[] args = Environment.GetCommandLineArgs();

        if (args.Length >= 2)
        {
            LoadImageFile(args[1]);
        }
        else
        {
            LoadClipboardImage();
        }
    }

    private void LoadImageFile(string fileName)
    {
        try
        {
            var source = new BitmapImage();

            source.BeginInit();
            source.UriSource = new Uri(
                Path.GetFullPath(fileName),
                UriKind.Absolute);
            source.CacheOption = BitmapCacheOption.OnLoad;
            source.EndInit();
            source.Freeze();

            imageView.Source = source;

            ResetTransform(source);
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Failed to open image.\n\n{fileName}\n\n{ex.Message}",
                "iiv_view",
                MessageBoxButton.OK,
                MessageBoxImage.Error);

            Close();
        }
    }

    private void LoadClipboardImage()
    {
        BitmapSource? source = null;
        if (Clipboard.ContainsImage())
        {
            source = Clipboard.GetImage();
        }

        if (source == null)
        {
            MessageBox.Show(
                "No image found in the clipboard.",
                "iiv_view",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
            Close();
            return;
        }

        imageView.Source = source;

        ResetTransform(source);
    }

    private void ResetTransform(BitmapSource? source = null)
    {
        if (source == null)
        {
            _scale = 1.0;

            ScaleTransform.ScaleX = _scale;
            ScaleTransform.ScaleY = _scale;

            TranslateTransform.X = 0;
            TranslateTransform.Y = 0;

            UpdateTitleScale();
            return;
        }

        // Ensure layout measurements are current.
        UpdateLayout();

        var parent = imageView.Parent as FrameworkElement ?? this;

        double availableWidth = parent.ActualWidth;
        double availableHeight = parent.ActualHeight;

        if (availableWidth <= 0 || availableHeight <= 0)
        {
            availableWidth = this.ActualWidth;
            availableHeight = this.ActualHeight;
        }

        // Convert pixel size to WPF logical units using DPI
        double imageLogicalWidth = source.PixelWidth * 96.0 / (source.DpiX > 0 ? source.DpiX : 96.0);
        double imageLogicalHeight = source.PixelHeight * 96.0 / (source.DpiY > 0 ? source.DpiY : 96.0);

        // Keep default scale behavior (1.0) unless you want to fit down large images.
        _scale = 1.0;

        // Apply scale so any rendered size or layout-affecting properties consider the scale.
        ScaleTransform.ScaleX = _scale;
        ScaleTransform.ScaleY = _scale;

        // Account for Image control margins when centering.
        var margin = imageView.Margin;
        double horizontalMargin = margin.Left + margin.Right;
        double verticalMargin = margin.Top + margin.Bottom;

        double tx = (availableWidth - imageLogicalWidth * _scale - horizontalMargin) / 2.0 + margin.Left;
        double ty = (availableHeight - imageLogicalHeight * _scale - verticalMargin) / 2.0 + margin.Top;

        if (tx < 0)
            tx = 0;
        if (ty < 0)
            ty = 0;

        if (!double.IsFinite(tx))
            tx = 0;
        if (!double.IsFinite(ty))
            ty = 0;

        TranslateTransform.X = Math.Round(tx);
        TranslateTransform.Y = Math.Round(ty);

        UpdateTitleScale();
    }

    private void UpdateTitleScale()
    {
        // show scale as percentage in title, e.g. "iiv_view - 100%"
        try
        {
            var pct = Math.Round(_scale * 100.0);
            Title = $"iiv_view - {pct}%";
        }
        catch
        {
            // ignore formatting errors
        }
    }

    private void OnMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        _dragStart = e.GetPosition(this);
        _startX = TranslateTransform.X;
        _startY = TranslateTransform.Y;

        CaptureMouse();
    }

    private void OnMouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        ReleaseMouseCapture();
    }

    private void OnMouseMove(object sender, MouseEventArgs e)
    {
        if (!IsMouseCaptured)
            return;

        var p = e.GetPosition(this);

        TranslateTransform.X =
            _startX + p.X - _dragStart.X;

        TranslateTransform.Y =
            _startY + p.Y - _dragStart.Y;
    }

    private void OnMouseWheel(object sender, MouseWheelEventArgs e)
    {
        if (imageView.Source == null)
            return;

        var oldScale = _scale;

        _scale *= e.Delta > 0
            ? 1.15
            : 1.0 / 1.15;

        _scale = Math.Clamp(_scale, 0.05, 20.0);

        var mouse = e.GetPosition(this);

        var imagePointX =
            (mouse.X - TranslateTransform.X) / oldScale;

        var imagePointY =
            (mouse.Y - TranslateTransform.Y) / oldScale;

        TranslateTransform.X =
            mouse.X - imagePointX * _scale;

        TranslateTransform.Y =
            mouse.Y - imagePointY * _scale;

        ScaleTransform.ScaleX = _scale;
        ScaleTransform.ScaleY = _scale;

        UpdateTitleScale();
    }

    private void OnKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape)
        {
            Close();
        }
        else if (e.Key == Key.D0 || e.Key == Key.NumPad0)
        {
            ResetTransform();
        }
    }
}
