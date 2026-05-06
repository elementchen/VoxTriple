using System.Runtime.InteropServices;

namespace Esp32BtMicConfig.Services;

/// <summary>
/// Simulates keyboard input using Win32 SendInput API.
/// </summary>
public static class KeyboardSimulator
{
    private const int INPUT_KEYBOARD = 1;
    private const uint KEYEVENTF_KEYUP = 0x0002;
    private const uint KEYEVENTF_EXTENDEDKEY = 0x0001;

    [StructLayout(LayoutKind.Sequential)]
    private struct INPUT
    {
        public uint type;
        public INPUTUNION union;
    }

    [StructLayout(LayoutKind.Explicit)]
    private struct INPUTUNION
    {
        [FieldOffset(0)] public KEYBDINPUT ki;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KEYBDINPUT
    {
        public ushort wVk;
        public ushort wScan;
        public uint dwFlags;
        public uint time;
        public IntPtr ExtraInfo;
    }

    [DllImport("user32.dll", SetLastError = true, EntryPoint = "SendInput")]
    private static extern uint SendInput(uint nInputs, [MarshalAs(UnmanagedType.LPArray)] INPUT[] pInputs, int cbSize);

    /// <summary>
    /// Press and release a key with optional modifiers.
    /// </summary>
    /// <param name="vkCode">Virtual key code.</param>
    /// <param name="modifier">Modifier bitmask matching BLE protocol.</param>
    public static void SimulateKeyPress(byte vkCode, byte modifier)
    {
        var inputs = new List<INPUT>();

        // Press modifiers down
        if ((modifier & 0x01) != 0) inputs.Add(MakeKeyDown(0xA2)); // LCtrl
        if ((modifier & 0x02) != 0) inputs.Add(MakeKeyDown(0xA0)); // LShift
        if ((modifier & 0x04) != 0) inputs.Add(MakeKeyDown(0xA4)); // LAlt
        if ((modifier & 0x08) != 0) inputs.Add(MakeKeyDown(0x5B)); // LWin
        if ((modifier & 0x10) != 0) inputs.Add(MakeKeyDown(0xA3)); // RCtrl
        if ((modifier & 0x20) != 0) inputs.Add(MakeKeyDown(0xA1)); // RShift
        if ((modifier & 0x40) != 0) inputs.Add(MakeKeyDown(0xA5)); // RAlt
        if ((modifier & 0x80) != 0) inputs.Add(MakeKeyDown(0x5C)); // RWin

        // Press and release the actual key
        inputs.Add(MakeKeyDown(vkCode));
        inputs.Add(MakeKeyUp(vkCode));

        // Release modifiers in reverse
        if ((modifier & 0x80) != 0) inputs.Add(MakeKeyUp(0x5C));
        if ((modifier & 0x40) != 0) inputs.Add(MakeKeyUp(0xA5));
        if ((modifier & 0x20) != 0) inputs.Add(MakeKeyUp(0xA1));
        if ((modifier & 0x10) != 0) inputs.Add(MakeKeyUp(0xA3));
        if ((modifier & 0x08) != 0) inputs.Add(MakeKeyUp(0x5B));
        if ((modifier & 0x04) != 0) inputs.Add(MakeKeyUp(0xA4));
        if ((modifier & 0x02) != 0) inputs.Add(MakeKeyUp(0xA0));
        if ((modifier & 0x01) != 0) inputs.Add(MakeKeyUp(0xA2));

        var arr = inputs.ToArray();
        SendInput((uint)arr.Length, arr, Marshal.SizeOf<INPUT>());
    }

    private static INPUT MakeKeyDown(ushort vk) => new()
    {
        type = INPUT_KEYBOARD,
        union = new INPUTUNION { ki = new KEYBDINPUT { wVk = vk, dwFlags = IsExtendedKey(vk) ? KEYEVENTF_EXTENDEDKEY : 0 } }
    };

    private static INPUT MakeKeyUp(ushort vk) => new()
    {
        type = INPUT_KEYBOARD,
        union = new INPUTUNION { ki = new KEYBDINPUT { wVk = vk, dwFlags = KEYEVENTF_KEYUP | (IsExtendedKey(vk) ? KEYEVENTF_EXTENDEDKEY : 0) } }
    };

    private static bool IsExtendedKey(ushort vk) => vk is
        0x21 or 0x22 or 0x23 or 0x24 or  // PgUp, PgDn, End, Home
        0x25 or 0x26 or 0x27 or 0x28 or  // Arrow keys
        0x2D or 0x2E or                   // Insert, Delete
        0x5B or 0x5C or 0x5D or          // LWin, RWin, Apps
        0xA2 or 0xA3 or 0xA4 or 0xA5;    // Ctrl/Alt right variants
}
