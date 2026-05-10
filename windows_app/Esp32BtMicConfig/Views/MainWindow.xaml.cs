using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Interop;
using Esp32BtMicConfig.ViewModels;

namespace Esp32BtMicConfig.Views;

public partial class MainWindow : Window
{
    private MainViewModel ViewModel => (MainViewModel)DataContext;
    private HwndSource? _hwndSource;
    private InputMethodState _prevImeState;
    private bool _keyCaptured;

    public MainWindow()
    {
        InitializeComponent();
        SourceInitialized += OnSourceInitialized;
    }

    private void OnSourceInitialized(object? sender, EventArgs e)
    {
        _hwndSource = HwndSource.FromHwnd(new WindowInteropHelper(this).Handle);
        _hwndSource?.AddHook(WndProcHook);
    }

    private const int WM_KEYDOWN     = 0x0100;
    private const int WM_SYSKEYDOWN  = 0x0104;
    private const int WM_GETDLGCODE  = 0x0087;
    private const int DLGC_WANTALLKEYS = 0x0004;
    private const int SCAN_RSHIFT    = 0x36;  // Right Shift scan code

    private IntPtr WndProcHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        try
        {
            // When capturing, tell Windows we want ALL keys (Tab, arrows, etc.)
            if (DataContext is MainViewModel vm && vm.IsCapturing && msg == WM_GETDLGCODE)
            {
                handled = true;
                return new IntPtr(DLGC_WANTALLKEYS);
            }

            if (DataContext is MainViewModel vm2 && vm2.IsCapturing && !_keyCaptured)
            {
                if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
                {
                    int flags = lParam.ToInt32();
                    if ((flags & (1 << 30)) != 0) return IntPtr.Zero; // skip repeats

                    byte vkCode = (byte)(wParam.ToInt32() & 0xFF);
                    if (vkCode != 0 && vkCode != 0xE5)
                    {
                        _keyCaptured = true;
                        // extended-key flag (ctrl/alt) OR Right Shift scan code
                        bool isExtended = (flags & (1 << 24)) != 0;
                        int sc = (flags >> 16) & 0xFF;
                        bool isRShift = (vkCode == 0x10 && sc == SCAN_RSHIFT);
                        vm2.HandleCapturedKey(vkCode, isExtended || isRShift);
                        Dispatcher.Invoke(() => InputMethod.Current.ImeState = _prevImeState);
                        handled = true;
                    }
                }
            }
        }
        catch { /* defensive */ }
        return IntPtr.Zero;
    }

    private void BeginKeyCapture()
    {
        _keyCaptured = false;
        _prevImeState = InputMethod.Current.ImeState;
        InputMethod.Current.ImeState = InputMethodState.Off;
    }

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e) { }

    protected override void OnClosed(EventArgs e)
    {
        if (_hwndSource != null)
        {
            _hwndSource.RemoveHook(WndProcHook);
            _hwndSource.Dispose();
            _hwndSource = null;
        }
        if (DataContext is MainViewModel vm) vm.Dispose();
        base.OnClosed(e);
    }

    private void Button1Capture_Click(object sender, RoutedEventArgs e)
    {
        BeginKeyCapture();
        ViewModel.BeginCapture(0);
    }

    private void Button2Capture_Click(object sender, RoutedEventArgs e)
    {
        BeginKeyCapture();
        ViewModel.BeginCapture(1);
    }

    private void Button3Capture_Click(object sender, RoutedEventArgs e)
    {
        BeginKeyCapture();
        ViewModel.BeginCapture(2);
    }
}

public class InvertBoolConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        => value is bool b ? !b : false;
    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        => value is bool b ? !b : false;
}
