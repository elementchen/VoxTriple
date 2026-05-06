using System.Text.Json.Serialization;

namespace Esp32BtMicConfig.Models;

/// <summary>
/// Represents a single button's key mapping configuration.
/// </summary>
public class ButtonMapping
{
    /// <summary>Virtual key code of the mapped key.</summary>
    [JsonPropertyName("vk_code")]
    public byte VkCode { get; set; }

    /// <summary>Modifier key bitmask (bit 0-7 per protocol spec).</summary>
    [JsonPropertyName("modifier")]
    public byte Modifier { get; set; }

    // --- Display helpers (not serialized) ---

    [JsonIgnore]
    public string VkCodeDisplay => $"0x{VkCode:X2}";

    [JsonIgnore]
    public string KeyName => KeyNameMap.TryGetValue(VkCode, out var name) ? name : $"VK_{VkCode:X2}";

    [JsonIgnore]
    public string ModifierDisplay
    {
        get
        {
            if (Modifier == 0) return "None";
            var parts = new List<string>();
            if ((Modifier & 0x01) != 0) parts.Add("LCtrl");
            if ((Modifier & 0x02) != 0) parts.Add("LShift");
            if ((Modifier & 0x04) != 0) parts.Add("LAlt");
            if ((Modifier & 0x08) != 0) parts.Add("LWin");
            if ((Modifier & 0x10) != 0) parts.Add("RCtrl");
            if ((Modifier & 0x20) != 0) parts.Add("RShift");
            if ((Modifier & 0x40) != 0) parts.Add("RAlt");
            if ((Modifier & 0x80) != 0) parts.Add("RWin");
            return string.Join(" + ", parts);
        }
    }

    public byte[] ToBytes() => [VkCode, Modifier];

    public static ButtonMapping FromBytes(byte vkCode, byte modifier) => new() { VkCode = vkCode, Modifier = modifier };

    /// <summary>Merge a modifier flag into the existing modifier byte.</summary>
    public void SetModifier(byte mask, bool on)
    {
        if (on)
            Modifier |= mask;
        else
            Modifier &= (byte)~mask;
    }

    public bool HasModifier(byte mask) => (Modifier & mask) != 0;

    private static readonly Dictionary<byte, string> KeyNameMap = new()
    {
        [0x08] = "Backspace", [0x09] = "Tab", [0x0D] = "Enter", [0x10] = "Shift",
        [0x11] = "Ctrl", [0x12] = "Alt", [0x13] = "Pause", [0x14] = "CapsLock",
        [0x1B] = "Escape", [0x20] = "Space", [0x21] = "PageUp", [0x22] = "PageDown",
        [0x23] = "End", [0x24] = "Home", [0x25] = "Left", [0x26] = "Up",
        [0x27] = "Right", [0x28] = "Down", [0x2C] = "PrintScreen", [0x2D] = "Insert",
        [0x2E] = "Delete",
        [0x30] = "0", [0x31] = "1", [0x32] = "2", [0x33] = "3", [0x34] = "4",
        [0x35] = "5", [0x36] = "6", [0x37] = "7", [0x38] = "8", [0x39] = "9",
        [0x41] = "A", [0x42] = "B", [0x43] = "C", [0x44] = "D", [0x45] = "E",
        [0x46] = "F", [0x47] = "G", [0x48] = "H", [0x49] = "I", [0x4A] = "J",
        [0x4B] = "K", [0x4C] = "L", [0x4D] = "M", [0x4E] = "N", [0x4F] = "O",
        [0x50] = "P", [0x51] = "Q", [0x52] = "R", [0x53] = "S", [0x54] = "T",
        [0x55] = "U", [0x56] = "V", [0x57] = "W", [0x58] = "X", [0x59] = "Y",
        [0x5A] = "Z",
        [0x5B] = "LWin", [0x5C] = "RWin", [0x5D] = "Apps",
        [0x60] = "Num0", [0x61] = "Num1", [0x62] = "Num2", [0x63] = "Num3",
        [0x64] = "Num4", [0x65] = "Num5", [0x66] = "Num6", [0x67] = "Num7",
        [0x68] = "Num8", [0x69] = "Num9",
        [0x6A] = "Multiply", [0x6B] = "Add", [0x6C] = "Separator",
        [0x6D] = "Subtract", [0x6E] = "Decimal", [0x6F] = "Divide",
        [0x70] = "F1", [0x71] = "F2", [0x72] = "F3", [0x73] = "F4",
        [0x74] = "F5", [0x75] = "F6", [0x76] = "F7", [0x77] = "F8",
        [0x78] = "F9", [0x79] = "F10", [0x7A] = "F11", [0x7B] = "F12",
        [0x90] = "NumLock", [0xA0] = "LShift", [0xA1] = "RShift",
        [0xA2] = "LCtrl", [0xA3] = "RCtrl", [0xA4] = "LAlt", [0xA5] = "RAlt",
    };
}
