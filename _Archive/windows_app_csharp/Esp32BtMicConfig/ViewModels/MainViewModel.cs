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
    private bool _autoStart;

    partial void OnAutoStartChanged(bool value)
    {
        SetStartupShortcut(value);
    }

    private static void SetStartupShortcut(bool enable)
    {
        string startupDir = Environment.GetFolderPath(Environment.SpecialFolder.Startup);
        string shortcutPath = System.IO.Path.Combine(startupDir, "VoxTriple.lnk");
        if (enable)
        {
            string exePath = Environment.ProcessPath ??
                System.IO.Path.Combine(System.AppContext.BaseDirectory, "VoxTriple.exe");
            // Use WScript.Shell to create shortcut
            try
            {
                dynamic shell = Activator.CreateInstance(Type.GetTypeFromProgID("WScript.Shell")!)!;
                dynamic shortcut = shell.CreateShortcut(shortcutPath);
                shortcut.TargetPath = exePath;
                shortcut.WorkingDirectory = System.IO.Path.GetDirectoryName(exePath) ?? "";
                shortcut.Description = "VoxTriple Bluetooth Mic Config";
                shortcut.Save();
            }
            catch { /* ignore — will retry on next launch */ }
        }
        else
        {
            try { System.IO.File.Delete(shortcutPath); } catch { }
        }
    }

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

    protected override void OnPropertyChanged(System.ComponentModel.PropertyChangedEventArgs e)
    {
        base.OnPropertyChanged(e);
        // Auto-refresh key display when modifier checkboxes change
        if (e.PropertyName != null && e.PropertyName.Contains("Mod"))
            RefreshAllKeyDisplays();
    }

    private void RefreshAllKeyDisplays()
    {
        Button1KeyName = BuildKeyDisplay(Button1VkCode, GetModifierForButton(0));
        Button2KeyName = BuildKeyDisplay(Button2VkCode, GetModifierForButton(1));
        Button3KeyName = BuildKeyDisplay(Button3VkCode, GetModifierForButton(2));
    }

    private byte GetModifierForButton(int idx)
    {
        byte mod = 0;
        switch (idx)
        {
            case 0:
                if (Button1ModLCtrl) mod|=1<<0; if (Button1ModLShift) mod|=1<<1; if (Button1ModLAlt) mod|=1<<2; if (Button1ModLWin) mod|=1<<3;
                if (Button1ModRCtrl) mod|=1<<4; if (Button1ModRShift) mod|=1<<5; if (Button1ModRAlt) mod|=1<<6; if (Button1ModRWin) mod|=1<<7; break;
            case 1:
                if (Button2ModLCtrl) mod|=1<<0; if (Button2ModLShift) mod|=1<<1; if (Button2ModLAlt) mod|=1<<2; if (Button2ModLWin) mod|=1<<3;
                if (Button2ModRCtrl) mod|=1<<4; if (Button2ModRShift) mod|=1<<5; if (Button2ModRAlt) mod|=1<<6; if (Button2ModRWin) mod|=1<<7; break;
            case 2:
                if (Button3ModLCtrl) mod|=1<<0; if (Button3ModLShift) mod|=1<<1; if (Button3ModLAlt) mod|=1<<2; if (Button3ModLWin) mod|=1<<3;
                if (Button3ModRCtrl) mod|=1<<4; if (Button3ModRShift) mod|=1<<5; if (Button3ModRAlt) mod|=1<<6; if (Button3ModRWin) mod|=1<<7; break;
        }
        return mod;
    }

    public MainViewModel()
    {
        _config = ConfigurationService.Load();
        _bleClient.ButtonEventReceived += OnButtonEventReceived;
        _bleClient.DeviceStatusChanged += OnDeviceStatusChanged;
        ApplyConfigToUi();
        AutoStart = _config.AutoStart;
        // Auto-reconnect BLE on startup if previously paired
        _ = AutoConnectIfConfiguredAsync();
    }

    private async Task AutoConnectIfConfiguredAsync()
    {
        if (_config.BleAddress == 0) return;
        ConnectionStatusText = "Auto-connecting BLE...";
        IsScanning = true;
        try
        {
            // Scan first to confirm device is advertising
            var addr = await BleGattClient.ScanForDeviceAsync();
            if (addr == null)
            {
                ConnectionStatusText = "ESP32_BT_MIC not found. Click Scan & Connect when ready.";
                return;
            }
            bool ok = await _bleClient.ConnectByAddressAsync(addr.Value);
            if (ok)
            {
                IsConnected = true;
                ConnectionStatusText = $"Auto-connected: {_bleClient.DeviceName}";
                _config.DeviceAddress = _bleClient.DeviceAddress;
                await ReadMappingsFromDeviceAsync();
            }
            else
            {
                ConnectionStatusText = $"Auto-connect failed. Click Scan & Connect.";
            }
        }
        catch { ConnectionStatusText = "Auto-connect error. Try Scan & Connect."; }
        finally { IsScanning = false; }
    }

    // --- Commands ---

    [RelayCommand]
    private async Task ScanAndConnectAsync()
    {
        IsScanning = true;
        ConnectionStatusText = "Scanning for ESP32_BT_MIC...";

        try
        {
            var addr = await BleGattClient.ScanForDeviceAsync();
            if (addr == null)
            {
                ConnectionStatusText = BleGattClient.LastError;
                return;
            }

            ConnectionStatusText = "Device found! Connecting...";
            var success = await _bleClient.ConnectByAddressAsync(addr.Value);
            if (!success)
            {
                ConnectionStatusText = $"Connect failed: {BleGattClient.LastError}";
                return;
            }

            IsConnected = true;
            ConnectionStatusText = $"Connected: {_bleClient.DeviceName}";
            _config.DeviceAddress = _bleClient.DeviceAddress;
            // Save BLE address for auto-reconnect next time
            if (BleGattClient.FoundBluetoothAddress.HasValue)
            {
                _config.BleAddress = BleGattClient.FoundBluetoothAddress.Value;
                SaveConfigFile();
            }
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
        _config.AutoStart = AutoStart;
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

    // --- Key Capture (Win32 raw VK codes, bypasses IME) ---

    public bool IsCapturing => _capturingIndex >= 0;

    public void BeginCapture(int buttonIndex)
    {
        _capturingIndex = buttonIndex;
        SetCapturing(buttonIndex, true);
    }

    /// <summary>Called from Win32 WndProc — raw VK code vs WPF IME-processed key.</summary>
    public void HandleCapturedKey(byte vkCode, bool isExtended = false)
    {
        if (_capturingIndex < 0) return;

        byte mappedVk = MapModifierVk(vkCode, isExtended);
        byte modifier = 0;

        bool isModifierKey = mappedVk is 0xA0 or 0xA1 or 0xA2 or 0xA3 or 0xA4 or 0xA5 or 0x5B or 0x5C;

        if (!isModifierKey)
        {
            if (Keyboard.IsKeyDown(Key.LeftCtrl))  modifier |= 1 << 0;
            if (Keyboard.IsKeyDown(Key.LeftShift)) modifier |= 1 << 1;
            if (Keyboard.IsKeyDown(Key.LeftAlt))   modifier |= 1 << 2;
            if (Keyboard.IsKeyDown(Key.LWin))      modifier |= 1 << 3;
            if (Keyboard.IsKeyDown(Key.RightCtrl)) modifier |= 1 << 4;
            if (Keyboard.IsKeyDown(Key.RightShift)) modifier |= 1 << 5;
            if (Keyboard.IsKeyDown(Key.RightAlt))  modifier |= 1 << 6;
            if (Keyboard.IsKeyDown(Key.RWin))      modifier |= 1 << 7;
        }

        var name = BuildKeyDisplay(mappedVk, modifier);
        ApplyKeyToButton(_capturingIndex, mappedVk, name, modifier);
        SetCapturing(_capturingIndex, false);
        _capturingIndex = -1;
    }

    private static byte MapModifierVk(byte vk, bool extended)
    {
        return vk switch
        {
            0x11 => extended ? (byte)0xA3 : (byte)0xA2,
            0x10 => extended ? (byte)0xA1 : (byte)0xA0,
            0x12 => extended ? (byte)0xA5 : (byte)0xA4,
            _ => vk
        };
    }

    private static string BuildKeyDisplay(byte vkCode, byte modifier)
    {
        var parts = new List<string>();
        if ((modifier & (1 << 0)) != 0) parts.Add("LCtrl");
        if ((modifier & (1 << 1)) != 0) parts.Add("LShift");
        if ((modifier & (1 << 2)) != 0) parts.Add("LAlt");
        if ((modifier & (1 << 3)) != 0) parts.Add("LWin");
        if ((modifier & (1 << 4)) != 0) parts.Add("RCtrl");
        if ((modifier & (1 << 5)) != 0) parts.Add("RShift");
        if ((modifier & (1 << 6)) != 0) parts.Add("RAlt");
        if ((modifier & (1 << 7)) != 0) parts.Add("RWin");
        parts.Add(new ButtonMapping { VkCode = vkCode }.KeyName);
        return string.Join("+", parts);
    }

    private void ApplyKeyToButton(int index, byte vkCode, string keyName, byte modifier = 0)
    {
        bool lc=(modifier&(1<<0))!=0,ls=(modifier&(1<<1))!=0,la=(modifier&(1<<2))!=0,lw=(modifier&(1<<3))!=0;
        bool rc=(modifier&(1<<4))!=0,rs=(modifier&(1<<5))!=0,ra=(modifier&(1<<6))!=0,rw=(modifier&(1<<7))!=0;
        switch (index)
        {
            case 0: Button1VkCode=vkCode;Button1KeyName=keyName;Button1ModLCtrl=lc;Button1ModLShift=ls;Button1ModLAlt=la;Button1ModLWin=lw;Button1ModRCtrl=rc;Button1ModRShift=rs;Button1ModRAlt=ra;Button1ModRWin=rw;break;
            case 1: Button2VkCode=vkCode;Button2KeyName=keyName;Button2ModLCtrl=lc;Button2ModLShift=ls;Button2ModLAlt=la;Button2ModLWin=lw;Button2ModRCtrl=rc;Button2ModRShift=rs;Button2ModRAlt=ra;Button2ModRWin=rw;break;
            case 2: Button3VkCode=vkCode;Button3KeyName=keyName;Button3ModLCtrl=lc;Button3ModLShift=ls;Button3ModLAlt=la;Button3ModLWin=lw;Button3ModRCtrl=rc;Button3ModRShift=rs;Button3ModRAlt=ra;Button3ModRWin=rw;break;
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
                ApplyMappingToUi(0, ButtonMapping.FromBytes(mappings.Value.V1, mappings.Value.M1));
                ApplyMappingToUi(1, ButtonMapping.FromBytes(mappings.Value.V2, mappings.Value.M2));
                ApplyMappingToUi(2, ButtonMapping.FromBytes(mappings.Value.V3, mappings.Value.M3));
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
            LastEventText = $"Button {e.ButtonId + 1} {(e.State == 1 ? "PRESSED" : "RELEASED")}";

            var mapping = e.ButtonId switch { 0 => BuildMapping(0), 1 => BuildMapping(1), 2 => BuildMapping(2), _ => null };
            if (mapping == null || mapping.VkCode == 0) return;

            if (e.State == 1)
                KeyboardSimulator.KeyDown(mapping.VkCode, mapping.Modifier);
            else
                KeyboardSimulator.KeyUp(mapping.VkCode, mapping.Modifier);
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
