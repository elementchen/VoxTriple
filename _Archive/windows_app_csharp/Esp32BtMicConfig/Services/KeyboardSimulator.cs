using System.Runtime.InteropServices;

namespace Esp32BtMicConfig.Services;

/// <summary>
/// Simulates keyboard input using Win32 SendInput API.
/// Uses both VK code and scan code for maximum compatibility.
/// </summary>
public static class KeyboardSimulator
{
    private const int INPUT_KEYBOARD = 1;
    private const uint KEYEVENTF_KEYDOWN    = 0x0000;
    private const uint KEYEVENTF_KEYUP      = 0x0002;
    private const uint KEYEVENTF_EXTENDEDKEY = 0x0001;
    private const uint KEYEVENTF_SCANCODE   = 0x0008;
    private const uint MAPVK_VK_TO_VSC      = 0;

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
        public IntPtr dwExtraInfo;
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

    [DllImport("user32.dll")]
    private static extern uint MapVirtualKey(uint uCode, uint uMapType);

    public static void SimulateKeyPress(byte vkCode, byte modifier)
    {
        KeyDown(vkCode, modifier);
        KeyUp(vkCode, modifier);
    }

    public static void KeyDown(byte vkCode, byte modifier)
    {
        var inputs = new INPUT[8];
        int n = 0;
        if ((modifier & 0x01) != 0) inputs[n++] = MkKey(0xA2, true);
        if ((modifier & 0x02) != 0) inputs[n++] = MkKey(0xA0, true);
        if ((modifier & 0x04) != 0) inputs[n++] = MkKey(0xA4, true);
        if ((modifier & 0x08) != 0) inputs[n++] = MkKey(0x5B, true);
        if ((modifier & 0x10) != 0) inputs[n++] = MkKey(0xA3, true);
        if ((modifier & 0x20) != 0) inputs[n++] = MkKey(0xA1, true);
        if ((modifier & 0x40) != 0) inputs[n++] = MkKey(0xA5, true);
        if ((modifier & 0x80) != 0) inputs[n++] = MkKey(0x5C, true);
        inputs[n++] = MkKey(vkCode, true);
        if (n > 0) SendInput((uint)n, inputs[..n], Marshal.SizeOf<INPUT>());
    }

    public static void KeyUp(byte vkCode, byte modifier)
    {
        var inputs = new INPUT[8];
        int n = 0;
        inputs[n++] = MkKey(vkCode, false);
        if ((modifier & 0x80) != 0) inputs[n++] = MkKey(0x5C, false);
        if ((modifier & 0x40) != 0) inputs[n++] = MkKey(0xA5, false);
        if ((modifier & 0x20) != 0) inputs[n++] = MkKey(0xA1, false);
        if ((modifier & 0x10) != 0) inputs[n++] = MkKey(0xA3, false);
        if ((modifier & 0x08) != 0) inputs[n++] = MkKey(0x5B, false);
        if ((modifier & 0x04) != 0) inputs[n++] = MkKey(0xA4, false);
        if ((modifier & 0x02) != 0) inputs[n++] = MkKey(0xA0, false);
        if ((modifier & 0x01) != 0) inputs[n++] = MkKey(0xA2, false);
        if (n > 0) SendInput((uint)n, inputs[..n], Marshal.SizeOf<INPUT>());
    }

    private static INPUT MkKey(ushort vk, bool down)
    {
        bool ext = vk is 0x21 or 0x22 or 0x23 or 0x24 or 0x25 or 0x26 or 0x27 or 0x28
                   or 0x2D or 0x2E or 0x5B or 0x5C or 0x5D or 0xA2 or 0xA3 or 0xA4 or 0xA5;
        uint flags = down ? KEYEVENTF_KEYDOWN : KEYEVENTF_KEYUP;
        if (ext) flags |= KEYEVENTF_EXTENDEDKEY;
        return new INPUT
        {
            type = INPUT_KEYBOARD,
            union = new INPUTUNION
            {
                ki = new KEYBDINPUT { wVk = vk, dwFlags = flags }
            }
        };
    }
}
