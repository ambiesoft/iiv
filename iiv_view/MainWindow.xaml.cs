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

        Loaded += (_, _) => LoadImage();

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

            ImageView.Source = source;

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
        if (!Clipboard.ContainsImage())
        {
            Close();
            return;
        }

        var source = Clipboard.GetImage();

        if (source == null)
        {
            Close();
            return;
        }

        ImageView.Source = source;

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
            return;
        }

        // Get the size of parent container
        UpdateLayout();

        var parent = ImageView.Parent as FrameworkElement ?? this;

        double availableWidth = parent.ActualWidth;
        double availableHeight = parent.ActualHeight;

        if (availableWidth <= 0 || availableHeight <= 0)
        {
            availableWidth = this.ActualWidth;
            availableHeight = this.ActualHeight;
        }

        // Pixel width to WPF logical width
        double imageLogicalWidth = source.PixelWidth * 96.0 / (source.DpiX > 0 ? source.DpiX : 96.0);
        double imageLogicalHeight = source.PixelHeight * 96.0 / (source.DpiY > 0 ? source.DpiY : 96.0);

        if (imageLogicalWidth <= 0 || imageLogicalHeight <= 0)
        {
            _scale = 1.0;
        }
        else
        {
            var fitScale = Math.Min(availableWidth / imageLogicalWidth, availableHeight / imageLogicalHeight);

            // Scale down large images to fit, do not scale up small images (maximum 1.0)
            _scale = Math.Min(1.0, fitScale);
            if (_scale <= 0)
                _scale = 1.0;
        }

        ScaleTransform.ScaleX = _scale;
        ScaleTransform.ScaleY = _scale;

        // Place the image as centrally as possible
        TranslateTransform.X = Math.Round((availableWidth - imageLogicalWidth * _scale) / 2.0);
        TranslateTransform.Y = Math.Round((availableHeight - imageLogicalHeight * _scale) / 2.0);
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
        if (ImageView.Source == null)
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
