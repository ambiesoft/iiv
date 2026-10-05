using System.IO;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Imaging;

namespace iiv_view;

public partial class MainWindow : Window
{
    private Point _dragStart;
    private double _startX;
    private double _startY;
    private double _scale = 1.0;

    public MainWindow()
    {
        InitializeComponent();

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
        

        MouseLeftButtonDown += OnMouseLeftButtonDown;
        MouseLeftButtonUp += OnMouseLeftButtonUp;
        MouseMove += OnMouseMove;
        MouseWheel += OnMouseWheel;
        KeyDown += OnKeyDown;
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

    // Plan (pseudocode):
    // 1. If no source: reset scale and transforms to defaults and return.
    // 2. Ensure layout is up-to-date (UpdateLayout).
    // 3. Determine available width/height from the image's parent or the window as fallback.
    // 4. Compute image logical width/height from pixel dimensions and DPI.
    // 5. Keep existing scaling policy (use _scale); ensure ScaleTransform is applied before computing translation.
    // 6. Account for the Image control's Margin when centering (subtract margins from available space and add left/top margin back).
    // 7. Compute centered translate X/Y = round((available - imageSize*scale - margins) / 2.0 + marginStart).
    // 8. Guard against non-finite results and apply the computed translate values.
    private void ResetTransform(BitmapSource? source = null)
    {
        if (source == null)
        {
            _scale = 1.0;

            ScaleTransform.ScaleX = _scale;
            ScaleTransform.ScaleY = _scale;

            TranslateTransform.X = 0;
            TranslateTransform.Y = 0;
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
        // Example: to scale down large images to fit the container but never scale up:
        // var fitScale = Math.Min(availableWidth / imageLogicalWidth, availableHeight / imageLogicalHeight);
        // _scale = Math.Min(1.0, fitScale);
        // Ensure a sane scale value
        if (!double.IsFinite(_scale) || _scale <= 0)
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

        if (!double.IsFinite(tx))
            tx = 0;
        if (!double.IsFinite(ty))
            ty = 0;

        //TranslateTransform.X = Math.Abs(Math.Round(tx));
        //TranslateTransform.Y = Math.Abs(Math.Round(ty));

        //TranslateTransform.X = Math.Abs(
        //    (availableWidth - imageLogicalWidth * _scale) * _scale / 2
        //    );
        //TranslateTransform.X = (availableWidth - imageLogicalWidth) / 2;
    
        //        TranslateTransform.Y = (availableHeight-imageLogicalHeight)/2;  
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
