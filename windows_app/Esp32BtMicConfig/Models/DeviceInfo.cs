using System.Text.Json.Serialization;

namespace Esp32BtMicConfig.Models;

/// <summary>
/// Stores the known device address and button mappings for persistence.
/// </summary>
public class DeviceConfig
{
    [JsonPropertyName("device_address")]
    public string DeviceAddress { get; set; } = string.Empty;

    [JsonPropertyName("button1")]
    public ButtonMapping Button1 { get; set; } = new();

    [JsonPropertyName("button2")]
    public ButtonMapping Button2 { get; set; } = new();

    [JsonPropertyName("button3")]
    public ButtonMapping Button3 { get; set; } = new();
}
