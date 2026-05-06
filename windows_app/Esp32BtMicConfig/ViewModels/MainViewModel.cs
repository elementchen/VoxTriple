using System.Windows;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Esp32BtMicConfig.Models;
using Esp32BtMicConfig.Services;

namespace Esp32BtMicConfig.ViewModels;

public partial class MainViewModel : ObservableObject, IDisposable
{
    private readonly BleGattClient _bleClient = new();
    private DeviceConfig _config;

    // --- Observable properties ---

    [ObservableProperty]
    private string _connectionStatusText = "Disconnected";

    [ObservableProperty]
    private bool _isConnected;

    [ObservableProperty]
    private bool _isScanning;

    [ObservableProperty]
    private string _lastEventText = "None";

    [ObservableProperty]
    private string _hfpStatusText = "Unknown";

    [ObservableProperty]
    private string _audioStatusText = "Unknown";

    // Button 1
    [ObservableProperty]
    private string _button1KeyName = "None";

    [ObservableProperty]
    private byte _button1VkCode;

    [ObservableProperty]
    private bool _button1ModLCtrl;

    [ObservableProperty]
    private bool _button1ModLShift;

    [ObservableProperty]
    private bool _button1ModLAlt;

    [ObservableProperty]
    private bool _button1ModLWin;

    [ObservableProperty]
    private bool _button1ModRCtrl;

    [ObservableProperty]
    private bool _button1ModRShift;

    [ObservableProperty]
    private bool _button1ModRAlt;

    [ObservableProperty]
    private bool _button1ModRWin;

    [ObservableProperty]
    private bool _button1Capturing;

    // Button 2
    [ObservableProperty]
    private string _button2KeyName = "None";

    [ObservableProperty]
    private byte _button2VkCode;

    [ObservableProperty]
    private bool _button2ModLCtrl;

    [ObservableProperty]
    private bool _button2ModLShift;

    [ObservableProperty]
    private bool _button2ModLAlt;

    [ObservableProperty]
    private bool _button2ModLWin;

    [ObservableProperty]
    private bool _button2ModRCtrl;

    [ObservableProperty]
    private bool _button2ModRShift;

    [ObservableProperty]
    private bool _button2ModRAlt;

    [ObservableProperty]
    private bool _button2ModRWin;

    [ObservableProperty]
    private bool _button2Capturing;

    // Button 3
    [ObservableProperty]
    private string _button3KeyName = "None";

    [ObservableProperty]
    private byte _button3VkCode;

    [ObservableProperty]
    private bool _button3ModLCtrl;

    [ObservableProperty]
    private bool _button3ModLShift;

    [ObservableProperty]
    private bool _button3ModLAlt;

    [ObservableProperty]
    private bool _button3ModLWin;

    [ObservableProperty]
    private bool _button3ModRCtrl;

    [ObservableProperty]
    private bool _button3ModRShift;

    [ObservableProperty]
    private bool _button3ModRAlt;

    [ObservableProperty]
    private bool _button3ModRWin;

    [ObservableProperty]
    private bool _button3Capturing;

    // Currently capturing button index (0-2, -1 = none)
    private int _capturingIndex = -1;

    public MainViewModel()
    {
        _config = ConfigurationService.Load();
        _bleClient.ButtonEventReceived += OnButtonEventReceived;
        _bleClient.DeviceStatusChanged += OnDeviceStatusChanged;
        ApplyConfigToUi();
    }

    // --- Commands ---

    [RelayCommand]
    private async Task ScanAndConnectAsync()
    {
        IsScanning = true;
        ConnectionStatusText = "Scanning...";

        try
        {
            var deviceInfo = await BleGattClient.FindDeviceAsync();
            if (deviceInfo == null)
            {
                ConnectionStatusText = "Device not found. Make sure ESP32_BT_MIC is powered on and nearby.";
                return;
            }

            ConnectionStatusText = "Connecting...";
            var success = await _bleClient.ConnectAsync(deviceInfo.Id);
            if (!success)
            {
                ConnectionStatusText = "Connection failed. Please try again.";
                return;
            }

            IsConnected = true;
            ConnectionStatusText = $"Connected: {_bleClient.DeviceName}";
            _config.DeviceAddress = _bleClient.DeviceAddress;

            // Read current mappings from device
            await ReadMappingsFromDeviceAsync();
        }
        catch (Exception ex)
        {
            ConnectionStatusText = $"Error: {ex.Message}";
        }
        finally
        {
            IsScanning = false;
        }
    }

    [RelayCommand]
    private void Disconnect()
    {
        _bleClient.Disconnect();
        IsConnected = false;
        ConnectionStatusText = "Disconnected";
    }

    [RelayCommand]
    private async Task SaveToDeviceAsync()
    {
        if (!IsConnected) return;

        try
        {
            var mapping1 = BuildMapping(0);
            var mapping2 = BuildMapping(1);
            var mapping3 = BuildMapping(2);

            var ok1 = await _bleClient.WriteButtonMappingAsync(0, mapping1.VkCode, mapping1.Modifier);
            var ok2 = await _bleClient.WriteButtonMappingAsync(1, mapping2.VkCode, mapping2.Modifier);
            var ok3 = await _bleClient.WriteButtonMappingAsync(2, mapping3.VkCode, mapping3.Modifier);

            if (ok1 && ok2 && ok3)
                ConnectionStatusText = "Mappings saved to device.";
            else
                ConnectionStatusText = "Failed to write some mappings to device.";
        }
        catch (Exception ex)
        {
            ConnectionStatusText = $"Error: {ex.Message}";
        }
    }

    [RelayCommand]
    private void SaveConfigFile()
    {
        _config.Button1 = BuildMapping(0);
        _config.Button2 = BuildMapping(1);
        _config.Button3 = BuildMapping(2);
        ConfigurationService.Save(_config);
        ConnectionStatusText = "Configuration saved to file.";
    }

    [RelayCommand]
    private void LoadConfigFile()
    {
        _config = ConfigurationService.Load();
        ApplyConfigToUi();
        ConnectionStatusText = "Configuration loaded from file.";
    }

    // --- Key capture ---

    public void BeginCapture(int buttonIndex)
    {
        _capturingIndex = buttonIndex;
        SetCapturing(buttonIndex, true);
    }

    public void HandleKeyDown(Key key, Key originalKey)
    {
        if (_capturingIndex < 0) return;

        // Treat Ctrl/Shift/Alt/Win as modifiers only (don't capture them alone)
        if (key is Key.LeftCtrl or Key.RightCtrl or Key.LeftShift or Key.RightShift
            or Key.LeftAlt or Key.RightAlt or Key.LWin or Key.RWin)
        {
            return;
        }

        var vkCode = (byte)KeyInterop.VirtualKeyFromKey(originalKey);
        var name = new ButtonMapping { VkCode = vkCode }.KeyName;

        ApplyKeyToButton(_capturingIndex, vkCode, name);
        SetCapturing(_capturingIndex, false);
        _capturingIndex = -1;
    }

    public void HandleKeyUp()
    {
        // We handle everything in KeyDown, so this is just for modifier display
    }

    private void ApplyKeyToButton(int index, byte vkCode, string keyName)
    {
        switch (index)
        {
            case 0: Button1VkCode = vkCode; Button1KeyName = keyName; break;
            case 1: Button2VkCode = vkCode; Button2KeyName = keyName; break;
            case 2: Button3VkCode = vkCode; Button3KeyName = keyName; break;
        }
    }

    private void SetCapturing(int index, bool capturing)
    {
        switch (index)
        {
            case 0: Button1Capturing = capturing; break;
            case 1: Button2Capturing = capturing; break;
            case 2: Button3Capturing = capturing; break;
        }
    }

    private ButtonMapping BuildMapping(int index)
    {
        byte vkCode, modifier;
        bool lc, ls, la, lw, rc, rs, ra, rw;

        switch (index)
        {
            case 0:
                vkCode = Button1VkCode;
                lc = Button1ModLCtrl; ls = Button1ModLShift; la = Button1ModLAlt; lw = Button1ModLWin;
                rc = Button1ModRCtrl; rs = Button1ModRShift; ra = Button1ModRAlt; rw = Button1ModRWin;
                break;
            case 1:
                vkCode = Button2VkCode;
                lc = Button2ModLCtrl; ls = Button2ModLShift; la = Button2ModLAlt; lw = Button2ModLWin;
                rc = Button2ModRCtrl; rs = Button2ModRShift; ra = Button2ModRAlt; rw = Button2ModRWin;
                break;
            default:
                vkCode = Button3VkCode;
                lc = Button3ModLCtrl; ls = Button3ModLShift; la = Button3ModLAlt; lw = Button3ModLWin;
                rc = Button3ModRCtrl; rs = Button3ModRShift; ra = Button3ModRAlt; rw = Button3ModRWin;
                break;
        }

        modifier = 0;
        if (lc) modifier |= 0x01;
        if (ls) modifier |= 0x02;
        if (la) modifier |= 0x04;
        if (lw) modifier |= 0x08;
        if (rc) modifier |= 0x10;
        if (rs) modifier |= 0x20;
        if (ra) modifier |= 0x40;
        if (rw) modifier |= 0x80;

        return new ButtonMapping { VkCode = vkCode, Modifier = modifier };
    }

    private void ApplyConfigToUi()
    {
        ApplyMappingToUi(0, _config.Button1);
        ApplyMappingToUi(1, _config.Button2);
        ApplyMappingToUi(2, _config.Button3);
    }

    private void ApplyMappingToUi(int index, ButtonMapping mapping)
    {
        var name = mapping.KeyName;
        switch (index)
        {
            case 0:
                Button1VkCode = mapping.VkCode; Button1KeyName = name;
                Button1ModLCtrl = mapping.HasModifier(0x01); Button1ModLShift = mapping.HasModifier(0x02);
                Button1ModLAlt = mapping.HasModifier(0x04); Button1ModLWin = mapping.HasModifier(0x08);
                Button1ModRCtrl = mapping.HasModifier(0x10); Button1ModRShift = mapping.HasModifier(0x20);
                Button1ModRAlt = mapping.HasModifier(0x40); Button1ModRWin = mapping.HasModifier(0x80);
                break;
            case 1:
                Button2VkCode = mapping.VkCode; Button2KeyName = name;
                Button2ModLCtrl = mapping.HasModifier(0x01); Button2ModLShift = mapping.HasModifier(0x02);
                Button2ModLAlt = mapping.HasModifier(0x04); Button2ModLWin = mapping.HasModifier(0x08);
                Button2ModRCtrl = mapping.HasModifier(0x10); Button2ModRShift = mapping.HasModifier(0x20);
                Button2ModRAlt = mapping.HasModifier(0x40); Button2ModRWin = mapping.HasModifier(0x80);
                break;
            case 2:
                Button3VkCode = mapping.VkCode; Button3KeyName = name;
                Button3ModLCtrl = mapping.HasModifier(0x01); Button3ModLShift = mapping.HasModifier(0x02);
                Button3ModLAlt = mapping.HasModifier(0x04); Button3ModLWin = mapping.HasModifier(0x08);
                Button3ModRCtrl = mapping.HasModifier(0x10); Button3ModRShift = mapping.HasModifier(0x20);
                Button3ModRAlt = mapping.HasModifier(0x40); Button3ModRWin = mapping.HasModifier(0x80);
                break;
        }
    }

    private async Task ReadMappingsFromDeviceAsync()
    {
        if (!IsConnected) return;

        try
        {
            var mappings = await _bleClient.ReadAllMappingsAsync();
            if (mappings.HasValue)
            {
                ApplyMappingToUi(0, ButtonMapping.FromBytes(mappings.Value.Vk1, mappings.Value.Mod1));
                ApplyMappingToUi(1, ButtonMapping.FromBytes(mappings.Value.Vk2, mappings.Value.Mod2));
                ApplyMappingToUi(2, ButtonMapping.FromBytes(mappings.Value.Vk3, mappings.Value.Mod3));
                ConnectionStatusText = "Connected - Mappings read from device.";
            }
        }
        catch
        {
            ConnectionStatusText = "Connected (could not read mappings from device).";
        }
    }

    private void OnButtonEventReceived(object? sender, (byte ButtonId, byte State) e)
    {
        Application.Current.Dispatcher.Invoke(() =>
        {
            var stateStr = e.State == 1 ? "PRESSED" : "RELEASED";
            LastEventText = $"Button {e.ButtonId + 1} {stateStr}";

            // Simulate keyboard if button pressed
            if (e.State == 1)
            {
                var mapping = e.ButtonId switch
                {
                    0 => BuildMapping(0),
                    1 => BuildMapping(1),
                    2 => BuildMapping(2),
                    _ => null
                };

                if (mapping != null && mapping.VkCode != 0)
                {
                    KeyboardSimulator.SimulateKeyPress(mapping.VkCode, mapping.Modifier);
                }
            }
        });
    }

    private void OnDeviceStatusChanged(object? sender, (byte HfpConnected, byte AudioActive) e)
    {
        Application.Current.Dispatcher.Invoke(() =>
        {
            HfpStatusText = e.HfpConnected == 1 ? "Connected" : "Disconnected";
            AudioStatusText = e.AudioActive == 1 ? "Active" : "Inactive";
        });
    }

    public void Dispose()
    {
        _bleClient.ButtonEventReceived -= OnButtonEventReceived;
        _bleClient.DeviceStatusChanged -= OnDeviceStatusChanged;
        _bleClient.Dispose();
    }
}
