using System.IO;
using System.Text.Json;
using Esp32BtMicConfig.Models;

namespace Esp32BtMicConfig.Services;

/// <summary>
/// Persists device configuration as JSON in %AppData%/Esp32BtMicConfig/config.json.
/// </summary>
public static class ConfigurationService
{
    private static readonly string ConfigDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Esp32BtMicConfig");

    private static readonly string ConfigFile = Path.Combine(ConfigDir, "config.json");

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
    };

    public static DeviceConfig Load()
    {
        try
        {
            if (!File.Exists(ConfigFile))
                return new DeviceConfig();

            var json = File.ReadAllText(ConfigFile);
            return JsonSerializer.Deserialize<DeviceConfig>(json, JsonOptions) ?? new DeviceConfig();
        }
        catch
        {
            return new DeviceConfig();
        }
    }

    public static void Save(DeviceConfig config)
    {
        Directory.CreateDirectory(ConfigDir);
        var json = JsonSerializer.Serialize(config, JsonOptions);
        File.WriteAllText(ConfigFile, json);
    }
}
