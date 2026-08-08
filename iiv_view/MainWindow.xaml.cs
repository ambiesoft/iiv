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

        Loaded += (_, _) => LoadClipboardImage();
        MouseLeftButtonDown += OnMouseLeftButtonDown;
        MouseLeftButtonUp += OnMouseLeftButtonUp;
        MouseMove += OnMouseMove;
        MouseWheel += OnMouseWheel;
        KeyDown += OnKeyDown;
    }

    private void LoadClipboardImage()
    {
        if (!Clipboard.ContainsImage())
            return;

        var source = Clipboard.GetImage();
        if (source == null)
            return;

        ImageView.Source = source;
        _scale = 1.0;
        ScaleTransform.ScaleX = _scale;
        ScaleTransform.ScaleY = _scale;
        TranslateTransform.X = 0;
        TranslateTransform.Y = 0;
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
        TranslateTransform.X = _startX + p.X - _dragStart.X;
        TranslateTransform.Y = _startY + p.Y - _dragStart.Y;
    }

    private void OnMouseWheel(object sender, MouseWheelEventArgs e)
    {
        if (ImageView.Source == null)
            return;

        var oldScale = _scale;
        _scale *= e.Delta > 0 ? 1.15 : 1.0 / 1.15;
        _scale = Math.Clamp(_scale, 0.05, 20.0);

        var mouse = e.GetPosition(this);

        // Zoom around the mouse position by adjusting the translation based on the old and new scale.
        var imagePointX = (mouse.X - TranslateTransform.X) / oldScale;
        var imagePointY = (mouse.Y - TranslateTransform.Y) / oldScale;

        TranslateTransform.X = mouse.X - imagePointX * _scale;
        TranslateTransform.Y = mouse.Y - imagePointY * _scale;

        ScaleTransform.ScaleX = _scale;
        ScaleTransform.ScaleY = _scale;
    }

    private void OnKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape)
            Close();

        if (e.Key == Key.D0 || e.Key == Key.NumPad0)
        {
            _scale = 1.0;
            ScaleTransform.ScaleX = _scale;
            ScaleTransform.ScaleY = _scale;
        }
    }
}
