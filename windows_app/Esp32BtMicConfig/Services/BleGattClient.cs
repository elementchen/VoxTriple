using System.Text;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

namespace Esp32BtMicConfig.Services;

/// <summary>
/// BLE GATT client for communicating with the ESP32_BT_MIC device.
/// Service UUID 0x1820, characteristics per protocol spec.
/// </summary>
public class BleGattClient : IDisposable
{
    // GATT Service UUID: 0x1820
    private static readonly Guid ServiceUuid = Guid.Parse("00001820-0000-1000-8000-00805F9B34FB");

    // Characteristic UUIDs
    private static readonly Guid Button1MapUuid = Guid.Parse("00002A01-0000-1000-8000-00805F9B34FB");
    private static readonly Guid Button2MapUuid = Guid.Parse("00002A02-0000-1000-8000-00805F9B34FB");
    private static readonly Guid Button3MapUuid = Guid.Parse("00002A03-0000-1000-8000-00805F9B34FB");
    private static readonly Guid ButtonEventUuid = Guid.Parse("00002A04-0000-1000-8000-00805F9B34FB");
    private static readonly Guid DeviceStatusUuid = Guid.Parse("00002A05-0000-1000-8000-00805F9B34FB");

    private BluetoothLEDevice? _device;
    private GattDeviceServicesResult? _servicesResult;
    private GattCharacteristic? _button1Char;
    private GattCharacteristic? _button2Char;
    private GattCharacteristic? _button3Char;
    private GattCharacteristic? _buttonEventChar;
    private GattCharacteristic? _deviceStatusChar;

    public bool IsConnected => _device != null;
    public string DeviceName => _device?.Name ?? string.Empty;
    public string DeviceAddress { get; private set; } = string.Empty;

    /// <summary>Raised when a button event is received from the device.</summary>
    public event EventHandler<(byte ButtonId, byte State)>? ButtonEventReceived;

    /// <summary>Raised when device status changes.</summary>
    public event EventHandler<(byte HfpConnected, byte AudioActive)>? DeviceStatusChanged;

    /// <summary>Scan for ESP32_BT_MIC devices and return the first match.</summary>
    public static async Task<DeviceInformation?> FindDeviceAsync()
    {
        // Use DeviceInformation.FindAllAsync with BLE device selector
        // This enumerates paired BLE devices
        try
        {
            var selector = "(System.Devices.DevObjectType:=5)";
            var devices = await DeviceInformation.FindAllAsync(selector);

            if (devices.Count > 0)
            {
                // Try each paired BLE device, check if it has our service
                foreach (var device in devices)
                {
                    try
                    {
                        using var bleDevice = await BluetoothLEDevice.FromIdAsync(device.Id);
                        if (bleDevice == null) continue;

                        // Check the name matches
                        if (device.Name.Contains("ESP32_BT_MIC", StringComparison.OrdinalIgnoreCase))
                        {
                            return device;
                        }

                        // Also try to get services and check for our service UUID
                        var services = await bleDevice.GetGattServicesAsync();
                        if (services.Status == GattCommunicationStatus.Success)
                        {
                            var hasService = services.Services.Any(s => s.Uuid == ServiceUuid);
                            if (hasService)
                            {
                                return device;
                            }
                        }
                    }
                    catch
                    {
                        // Skip devices that fail to connect
                    }
                }
            }

            // If no paired device found, try a broader search with a watcher approach
            // using DeviceWatcher for unpaired devices
            return await FindDeviceByWatcherAsync();
        }
        catch
        {
            return null;
        }
    }

    private static async Task<DeviceInformation?> FindDeviceByWatcherAsync()
    {
        // Use DeviceWatcher for broader BLE device discovery
        var selector = "(System.Devices.DevObjectType:=5) AND (System.ItemNameDisplay:~~\"ESP32_BT_MIC\")";

        DeviceInformation? found = null;
        var tcs = new TaskCompletionSource<DeviceInformation?>();

        var watcher = DeviceInformation.CreateWatcher(selector,
            new[] { "System.Devices.DeviceAddress", "System.ItemNameDisplay" },
            DeviceInformationKind.AssociationEndpoint);

        watcher.Added += (_, args) =>
        {
            if (found == null && args.Name.Contains("ESP32_BT_MIC", StringComparison.OrdinalIgnoreCase))
            {
                found = args;
                tcs.TrySetResult(found);
            }
        };

        watcher.EnumerationCompleted += (_, _) => tcs.TrySetResult(found);
        watcher.Stopped += (_, _) => tcs.TrySetResult(found);

        watcher.Start();

        // Wait up to 10 seconds for discovery
        var timeout = Task.Delay(10000);
        var completed = await Task.WhenAny(tcs.Task, timeout);
        watcher.Stop();

        return found;
    }

    /// <summary>Connect to a device by its device ID (from DeviceInformation).</summary>
    public async Task<bool> ConnectAsync(string deviceId)
    {
        try
        {
            _device = await BluetoothLEDevice.FromIdAsync(deviceId);
            if (_device == null) return false;

            DeviceAddress = _device.DeviceId;
            _servicesResult = await _device.GetGattServicesAsync();
            if (_servicesResult.Status != GattCommunicationStatus.Success)
                return false;

            var service = _servicesResult.Services.FirstOrDefault(s => s.Uuid == ServiceUuid);
            if (service == null) return false;

            _button1Char = service.GetCharacteristics(Button1MapUuid).FirstOrDefault();
            _button2Char = service.GetCharacteristics(Button2MapUuid).FirstOrDefault();
            _button3Char = service.GetCharacteristics(Button3MapUuid).FirstOrDefault();
            _buttonEventChar = service.GetCharacteristics(ButtonEventUuid).FirstOrDefault();
            _deviceStatusChar = service.GetCharacteristics(DeviceStatusUuid).FirstOrDefault();

            // Subscribe to button events
            if (_buttonEventChar != null)
            {
                await _buttonEventChar.WriteClientCharacteristicConfigurationDescriptorAsync(
                    GattClientCharacteristicConfigurationDescriptorValue.Notify);
                _buttonEventChar.ValueChanged += OnButtonEventValueChanged;
            }

            // Subscribe to device status
            if (_deviceStatusChar != null)
            {
                await _deviceStatusChar.WriteClientCharacteristicConfigurationDescriptorAsync(
                    GattClientCharacteristicConfigurationDescriptorValue.Notify);
                _deviceStatusChar.ValueChanged += OnDeviceStatusValueChanged;
            }

            _device.ConnectionStatusChanged += OnConnectionStatusChanged;

            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>Connect using a BLE address (for reconnecting from saved config).</summary>
    public async Task<bool> ConnectByAddressAsync(string address)
    {
        // Convert "AA:BB:CC:DD:EE:FF" format or "0xXXXXXXXXXXXX" format
        if (string.IsNullOrEmpty(address)) return false;

        try
        {
            ulong macAddress;
            if (address.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            {
                macAddress = Convert.ToUInt64(address, 16);
            }
            else
            {
                // Parse XX:XX:XX:XX:XX:XX format
                var cleanAddress = address.Replace(":", "").Replace("-", "");
                macAddress = Convert.ToUInt64(cleanAddress, 16);
            }

            var deviceId = BluetoothAddressToId(macAddress);
            return await ConnectAsync(deviceId);
        }
        catch
        {
            return false;
        }
    }

    /// <summary>Write a button mapping to the device.</summary>
    public async Task<bool> WriteButtonMappingAsync(int buttonIndex, byte vkCode, byte modifier)
    {
        GattCharacteristic? targetChar = buttonIndex switch
        {
            0 => _button1Char,
            1 => _button2Char,
            2 => _button3Char,
            _ => null
        };

        if (targetChar == null) return false;

        var data = new byte[] { vkCode, modifier };
        var writer = new DataWriter();
        writer.WriteBytes(data);
        var buffer = writer.DetachBuffer();

        var result = await targetChar.WriteValueAsync(buffer, GattWriteOption.WriteWithResponse);
        return result == GattCommunicationStatus.Success;
    }

    /// <summary>Read a button mapping from the device.</summary>
    public async Task<(byte VkCode, byte Modifier)?> ReadButtonMappingAsync(int buttonIndex)
    {
        GattCharacteristic? targetChar = buttonIndex switch
        {
            0 => _button1Char,
            1 => _button2Char,
            2 => _button3Char,
            _ => null
        };

        if (targetChar == null) return null;

        var result = await targetChar.ReadValueAsync();
        if (result.Status != GattCommunicationStatus.Success) return null;

        var reader = DataReader.FromBuffer(result.Value);
        if (reader.UnconsumedBufferLength < 2) return null;

        return (reader.ReadByte(), reader.ReadByte());
    }

    /// <summary>Read all three button mappings from the device.</summary>
    public async Task<(byte Vk1, byte Mod1, byte Vk2, byte Mod2, byte Vk3, byte Mod3)?> ReadAllMappingsAsync()
    {
        var b1 = await ReadButtonMappingAsync(0);
        var b2 = await ReadButtonMappingAsync(1);
        var b3 = await ReadButtonMappingAsync(2);

        if (b1 == null || b2 == null || b3 == null) return null;

        return (b1.Value.VkCode, b1.Value.Modifier,
                b2.Value.VkCode, b2.Value.Modifier,
                b3.Value.VkCode, b3.Value.Modifier);
    }

    private void OnButtonEventValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        var reader = DataReader.FromBuffer(args.CharacteristicValue);
        if (reader.UnconsumedBufferLength >= 2)
        {
            var buttonId = reader.ReadByte();
            var state = reader.ReadByte();
            ButtonEventReceived?.Invoke(this, (buttonId, state));
        }
    }

    private void OnDeviceStatusValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        var reader = DataReader.FromBuffer(args.CharacteristicValue);
        if (reader.UnconsumedBufferLength >= 2)
        {
            var hfp = reader.ReadByte();
            var audio = reader.ReadByte();
            DeviceStatusChanged?.Invoke(this, (hfp, audio));
        }
    }

    private void OnConnectionStatusChanged(BluetoothLEDevice sender, object args)
    {
        if (sender.ConnectionStatus == BluetoothConnectionStatus.Disconnected)
        {
            Disconnect();
        }
    }

    public void Disconnect()
    {
        if (_buttonEventChar != null)
            _buttonEventChar.ValueChanged -= OnButtonEventValueChanged;
        if (_deviceStatusChar != null)
            _deviceStatusChar.ValueChanged -= OnDeviceStatusValueChanged;

        if (_device != null)
        {
            _device.ConnectionStatusChanged -= OnConnectionStatusChanged;
            _device.Dispose();
            _device = null;
        }

        _servicesResult = null;
        _button1Char = _button2Char = _button3Char = _buttonEventChar = _deviceStatusChar = null;
    }

    private static string BluetoothAddressToId(ulong address)
    {
        // Windows expects a specific device ID format for BLE
        return $"BluetoothLE#BluetoothLE{address:x}";
    }

    public void Dispose()
    {
        Disconnect();
        GC.SuppressFinalize(this);
    }
}
