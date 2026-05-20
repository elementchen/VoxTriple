using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Storage.Streams;

namespace Esp32BtMicConfig.Services;

/// <summary>
/// BLE GATT client for ESP32_BT_MIC button mapping.
/// Uses BluetoothLEAdvertisementWatcher for scanning (proven reliable
/// in the WiFi sister project) and retry logic for GATT discovery.
/// </summary>
public class BleGattClient : IDisposable
{
    private static readonly Guid ServiceUuid       = Guid.Parse("00001820-0000-1000-8000-00805F9B34FB");
    private static readonly Guid Button1MapUuid    = Guid.Parse("00002A01-0000-1000-8000-00805F9B34FB");
    private static readonly Guid Button2MapUuid    = Guid.Parse("00002A02-0000-1000-8000-00805F9B34FB");
    private static readonly Guid Button3MapUuid    = Guid.Parse("00002A03-0000-1000-8000-00805F9B34FB");
    private static readonly Guid ButtonEventUuid   = Guid.Parse("00002A04-0000-1000-8000-00805F9B34FB");
    private static readonly Guid DeviceStatusUuid  = Guid.Parse("00002A05-0000-1000-8000-00805F9B34FB");
    private static readonly Guid TxPowerUuid       = Guid.Parse("00002A06-0000-1000-8000-00805F9B34FB");
    private static readonly Guid SleepModeUuid     = Guid.Parse("00002A07-0000-1000-8000-00805F9B34FB");

    private BluetoothLEDevice? _device;
    private GattCharacteristic? _button1Char, _button2Char, _button3Char;
    private GattCharacteristic? _buttonEventChar, _deviceStatusChar;
    private GattCharacteristic? _txPowerChar, _sleepModeChar;

    public bool IsConnected => _device != null;
    public string DeviceName => _device?.Name ?? string.Empty;
    public string DeviceAddress { get; private set; } = string.Empty;
    public static string LastError { get; private set; } = "";
    public static ulong? FoundBluetoothAddress { get; private set; }

    public event EventHandler<(byte ButtonId, byte State)>? ButtonEventReceived;
    public event EventHandler<(byte HfpConnected, byte AudioActive)>? DeviceStatusChanged;

    /// <summary>Scan for ESP32_BT_MIC using BLE advertisement watcher.</summary>
    public static async Task<ulong?> ScanForDeviceAsync()
    {
        FoundBluetoothAddress = null;
        LastError = "";
        var tcs = new TaskCompletionSource<ulong?>();

        var watcher = new BluetoothLEAdvertisementWatcher
        {
            ScanningMode = BluetoothLEScanningMode.Active,
        };

        watcher.Received += (_, args) =>
        {
            string? name = args.Advertisement.LocalName;
            if (string.IsNullOrEmpty(name)) return;
            if (name.Contains("ESP32", StringComparison.OrdinalIgnoreCase))
            {
                FoundBluetoothAddress = args.BluetoothAddress;
                tcs.TrySetResult(args.BluetoothAddress);
            }
        };

        watcher.Stopped += (_, _) => tcs.TrySetResult(null);
        watcher.Start();

        var timeout = Task.Delay(10000);
        await Task.WhenAny(tcs.Task, timeout);
        watcher.Stop();

        if (FoundBluetoothAddress == null)
            LastError = "ESP32_BT_MIC not found. Verify ESP32 is powered on and advertising.";
        return FoundBluetoothAddress;
    }

    /// <summary>Connect by Bluetooth MAC address.</summary>
    public async Task<bool> ConnectByAddressAsync(ulong address)
    {
        try
        {
            Disconnect();
            _device = await BluetoothLEDevice.FromBluetoothAddressAsync(address);
            if (_device == null) { LastError = "BluetoothLEDevice returned null"; return false; }
            DeviceAddress = _device.DeviceId;
            return await ConnectInternalAsync();
        }
        catch (Exception ex) { LastError = ex.Message; return false; }
    }

    private async Task<bool> ConnectInternalAsync()
    {
        if (_device == null) { LastError = "_device null"; return false; }

        LastError = "Waiting for GATT services...";
        await Task.Delay(1500);

        // Retry GATT service discovery
        GattDeviceServicesResult? svcResult = null;
        for (int r = 0; r < 3; r++)
        {
            svcResult = await _device.GetGattServicesAsync(BluetoothCacheMode.Uncached);
            LastError = $"GATT svc attempt {r + 1}: {svcResult.Status}";
            if (svcResult.Status == GattCommunicationStatus.Success) break;
            if (svcResult.Status == GattCommunicationStatus.Unreachable)
            {
                LastError += " (unreachable — is device in range?)";
                return false;
            }
            await Task.Delay(1000);
        }

        if (svcResult?.Status != GattCommunicationStatus.Success)
        {
            LastError = $"GATT service discovery failed after 3 retries: {svcResult?.Status}";
            return false;
        }

        LastError = $"Found {svcResult.Services.Count} services";
        var service = svcResult.Services.FirstOrDefault(s => s.Uuid == ServiceUuid);
        if (service == null)
        {
            LastError = $"Service 0x1820 not found. Available: {string.Join(", ", svcResult.Services.Select(s => s.Uuid.ToString().Substring(4, 4)))}";
            return false;
        }

        // Retry characteristic discovery
        GattCharacteristicsResult? charResult = null;
        for (int r = 0; r < 3; r++)
        {
            charResult = await service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached);
            LastError = $"Char attempt {r + 1}: {charResult.Status} (err={charResult.ProtocolError})";
            if (charResult.Status == GattCommunicationStatus.Success) break;
            if (charResult.Status == GattCommunicationStatus.Unreachable) return false;
            await Task.Delay(800);
        }

        if (charResult?.Status != GattCommunicationStatus.Success)
        {
            LastError = $"Char discovery failed: {charResult?.Status}";
            return false;
        }

        var allChars = charResult.Characteristics;
        LastError = $"Service has {allChars.Count} characteristics";

        _button1Char    = allChars.FirstOrDefault(c => c.Uuid == Button1MapUuid);
        _button2Char    = allChars.FirstOrDefault(c => c.Uuid == Button2MapUuid);
        _button3Char    = allChars.FirstOrDefault(c => c.Uuid == Button3MapUuid);
        _buttonEventChar  = allChars.FirstOrDefault(c => c.Uuid == ButtonEventUuid);
        _deviceStatusChar = allChars.FirstOrDefault(c => c.Uuid == DeviceStatusUuid);
        _txPowerChar      = allChars.FirstOrDefault(c => c.Uuid == TxPowerUuid);
        _sleepModeChar    = allChars.FirstOrDefault(c => c.Uuid == SleepModeUuid);

        // Subscribe
        if (_buttonEventChar != null)
        {
            try
            {
                var r = await _buttonEventChar.WriteClientCharacteristicConfigurationDescriptorAsync(
                    GattClientCharacteristicConfigurationDescriptorValue.Notify);
                _buttonEventChar.ValueChanged += OnButtonEvent;
                LastError += $" | evt_cccd={r}";
            }
            catch (Exception ex) { LastError += $" | evt_sub_err={ex.Message}"; }
        }

        if (_deviceStatusChar != null)
        {
            try
            {
                await _deviceStatusChar.WriteClientCharacteristicConfigurationDescriptorAsync(
                    GattClientCharacteristicConfigurationDescriptorValue.Notify);
                _deviceStatusChar.ValueChanged += OnDeviceStatus;
            }
            catch { /* non-critical */ }
        }

        _device.ConnectionStatusChanged += OnConnectionChanged;
        LastError += $" | btn1={_button1Char!=null} btn2={_button2Char!=null} btn3={_button3Char!=null} evt={_buttonEventChar!=null} st={_deviceStatusChar!=null} tx={_txPowerChar!=null} sl={_sleepModeChar!=null}";
        return true;
    }

    public async Task<bool> WriteButtonMappingAsync(int idx, byte vk, byte mod)
    {
        var ch = idx switch { 0 => _button1Char, 1 => _button2Char, 2 => _button3Char, _ => null };
        if (ch == null) return false;
        var w = new DataWriter();
        w.WriteBytes(new[] { vk, mod });
        var r = await ch.WriteValueAsync(w.DetachBuffer(), GattWriteOption.WriteWithResponse);
        return r == GattCommunicationStatus.Success;
    }

    public async Task<(byte Vk, byte Mod)?> ReadButtonMappingAsync(int idx)
    {
        var ch = idx switch { 0 => _button1Char, 1 => _button2Char, 2 => _button3Char, _ => null };
        if (ch == null) return null;
        var r = await ch.ReadValueAsync();
        if (r.Status != GattCommunicationStatus.Success) return null;
        var d = DataReader.FromBuffer(r.Value);
        if (d.UnconsumedBufferLength < 2) return null;
        return (d.ReadByte(), d.ReadByte());
    }

    public async Task<(byte V1, byte M1, byte V2, byte M2, byte V3, byte M3)?> ReadAllMappingsAsync()
    {
        var a = await ReadButtonMappingAsync(0); if (a == null) return null;
        var b = await ReadButtonMappingAsync(1); if (b == null) return null;
        var c = await ReadButtonMappingAsync(2); if (c == null) return null;
        return (a.Value.Vk, a.Value.Mod, b.Value.Vk, b.Value.Mod, c.Value.Vk, c.Value.Mod);
    }

    public async Task<bool> WriteTxPowerAsync(byte level)
    {
        if (_txPowerChar == null) return false;
        var w = new DataWriter();
        w.WriteByte(level);
        var r = await _txPowerChar.WriteValueAsync(w.DetachBuffer(), GattWriteOption.WriteWithResponse);
        return r == GattCommunicationStatus.Success;
    }

    public async Task<byte?> ReadTxPowerAsync()
    {
        if (_txPowerChar == null) return null;
        var r = await _txPowerChar.ReadValueAsync();
        if (r.Status != GattCommunicationStatus.Success) return null;
        var d = DataReader.FromBuffer(r.Value);
        if (d.UnconsumedBufferLength < 1) return null;
        return d.ReadByte();
    }

    public async Task<bool> WriteSleepModeAsync(byte enabled)
    {
        if (_sleepModeChar == null) return false;
        var w = new DataWriter();
        w.WriteByte(enabled);
        var r = await _sleepModeChar.WriteValueAsync(w.DetachBuffer(), GattWriteOption.WriteWithResponse);
        return r == GattCommunicationStatus.Success;
    }

    public async Task<byte?> ReadSleepModeAsync()
    {
        if (_sleepModeChar == null) return null;
        var r = await _sleepModeChar.ReadValueAsync();
        if (r.Status != GattCommunicationStatus.Success) return null;
        var d = DataReader.FromBuffer(r.Value);
        if (d.UnconsumedBufferLength < 1) return null;
        return d.ReadByte();
    }

    private void OnButtonEvent(GattCharacteristic s, GattValueChangedEventArgs a)
    {
        var d = DataReader.FromBuffer(a.CharacteristicValue);
        if (d.UnconsumedBufferLength >= 2)
            ButtonEventReceived?.Invoke(this, (d.ReadByte(), d.ReadByte()));
    }

    private void OnDeviceStatus(GattCharacteristic s, GattValueChangedEventArgs a)
    {
        var d = DataReader.FromBuffer(a.CharacteristicValue);
        if (d.UnconsumedBufferLength >= 2)
            DeviceStatusChanged?.Invoke(this, (d.ReadByte(), d.ReadByte()));
    }

    private void OnConnectionChanged(BluetoothLEDevice s, object _)
    {
        if (s.ConnectionStatus == BluetoothConnectionStatus.Disconnected)
            Disconnect();
    }

    public void Disconnect()
    {
        if (_buttonEventChar != null) _buttonEventChar.ValueChanged -= OnButtonEvent;
        if (_deviceStatusChar != null) _deviceStatusChar.ValueChanged -= OnDeviceStatus;
        if (_device != null) { _device.ConnectionStatusChanged -= OnConnectionChanged; _device.Dispose(); _device = null; }
        _button1Char = _button2Char = _button3Char = _buttonEventChar = _deviceStatusChar = _txPowerChar = _sleepModeChar = null;
    }

    public void Dispose() { Disconnect(); GC.SuppressFinalize(this); }
}
