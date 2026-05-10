using System.Runtime.InteropServices;

namespace Esp32BtMicConfig.Services;

/// <summary>
/// Simulates keyboard input using Win32 keybd_event API.
/// Uses keybd_event (not SendInput) — proven reliable in WiFi sister project.
/// keybd_event manipulates the system-wide keyboard state, avoiding
/// the per-thread input queue issues that SendInput has with UIPI/focus.
/// </summary>
public static class KeyboardSimulator
{
    [DllImport("user32.dll")]
    private static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

    private const uint KEYEVENTF_KEYDOWN = 0x0000;
    private const uint KEYEVENTF_KEYUP   = 0x0002;

    /// <summary>Press and release a key with modifiers (single shot).</summary>
    public static void SimulateKeyPress(byte vkCode, byte modifier)
    {
        KeyDown(vkCode, modifier);
        KeyUp(vkCode, modifier);
    }

    /// <summary>Press key + modifiers down. Call KeyUp to release.</summary>
    public static void KeyDown(byte vkCode, byte modifier)
    {
        try
        {
            if ((modifier & 0x01) != 0) keybd_event(0xA2, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
            if ((modifier & 0x02) != 0) keybd_event(0xA0, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
            if ((modifier & 0x04) != 0) keybd_event(0xA4, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
            if ((modifier & 0x08) != 0) keybd_event(0x5B, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
            if ((modifier & 0x10) != 0) keybd_event(0xA3, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
            if ((modifier & 0x20) != 0) keybd_event(0xA1, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
            if ((modifier & 0x40) != 0) keybd_event(0xA5, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
            if ((modifier & 0x80) != 0) keybd_event(0x5C, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);

            keybd_event(vkCode, 0, KEYEVENTF_KEYDOWN, UIntPtr.Zero);
        }
        catch { /* ignore */ }
    }

    /// <summary>Release key + modifiers previously pressed by KeyDown.</summary>
    public static void KeyUp(byte vkCode, byte modifier)
    {
        try
        {
            keybd_event(vkCode, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);

            if ((modifier & 0x80) != 0) keybd_event(0x5C, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            if ((modifier & 0x40) != 0) keybd_event(0xA5, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            if ((modifier & 0x20) != 0) keybd_event(0xA1, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            if ((modifier & 0x10) != 0) keybd_event(0xA3, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            if ((modifier & 0x08) != 0) keybd_event(0x5B, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            if ((modifier & 0x04) != 0) keybd_event(0xA4, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            if ((modifier & 0x02) != 0) keybd_event(0xA0, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
            if ((modifier & 0x01) != 0) keybd_event(0xA2, 0, KEYEVENTF_KEYUP, UIntPtr.Zero);
        }
        catch { /* ignore */ }
    }
}
