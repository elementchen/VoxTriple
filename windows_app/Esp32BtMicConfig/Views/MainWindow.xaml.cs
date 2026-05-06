using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Input;
using Esp32BtMicConfig.ViewModels;

namespace Esp32BtMicConfig.Views;

public partial class MainWindow : Window
{
    private MainViewModel ViewModel => (MainViewModel)DataContext;

    public MainWindow()
    {
        InitializeComponent();
    }

    private void Window_PreviewKeyDown(object sender, KeyEventArgs e)
    {
        // If a button capture is active, forward the key to the VM
        if (DataContext is MainViewModel vm)
        {
            // First check for modifiers - show them in UI but don't finalize capture
            // The actual key is captured when a non-modifier key is pressed
            vm.HandleKeyDown(e.Key, e.Key);
            e.Handled = true;
        }
    }

    private void Window_Closing(object? sender, System.ComponentModel.CancelEventArgs e)
    {
        if (DataContext is MainViewModel vm)
        {
            vm.Dispose();
        }
    }

    private void Button1Capture_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.BeginCapture(0);
    }

    private void Button2Capture_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.BeginCapture(1);
    }

    private void Button3Capture_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.BeginCapture(2);
    }
}

/// <summary>
/// Converts a boolean and inverts it. Used to disable the Capture button while capturing is active.
/// </summary>
public class InvertBoolConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is bool b) return !b;
        return false;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is bool b) return !b;
        return false;
    }
}
