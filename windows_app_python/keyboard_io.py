"""Keyboard capture via pynput (reliable cross-platform listener)
and simulation via Win32 keybd_event (proven in C# version).

Capture: pynput.keyboard.Listener wraps WH_KEYBOARD_LL on Windows
and handles threading correctly. It provides raw VK codes via the
win32_event_filter callback, bypassing IME.

Simulation: ctypes → user32.keybd_event, same as C# version.
"""
import ctypes
import logging
from pynput import keyboard

log = logging.getLogger(__name__)

# ── Win32 keybd_event (for simulation) ──────────────────────────
user32 = ctypes.windll.user32
KEYEVENTF_KEYDOWN = 0x0000
KEYEVENTF_KEYUP   = 0x0002

# Modifier VK codes
VK_LCtrl  = 0xA2; VK_RCtrl  = 0xA3
VK_LShift = 0xA0; VK_RShift = 0xA1
VK_LAlt   = 0xA4; VK_RAlt   = 0xA5
VK_LWin   = 0x5B; VK_RWin   = 0x5C
SCAN_RSHIFT = 0x36

# ── Key capture via pynput ──────────────────────────────────────
_listener: keyboard.Listener | None = None
_capture_callback = None  # called with (vk_code, is_extended, scan_code)


def start_key_capture(callback, tk_root=None):
    """Start global keyboard listener. `callback(vk, is_extended, scan_code)`.
    The listener's win32_event_filter provides raw VK codes before IME."""
    global _listener, _capture_callback
    _capture_callback = callback

    def on_win32_event(msg, data):
        """pynput win32_event_filter. `data` is the KBDLLHOOKSTRUCT (value, not pointer).
        We extract raw VK code + extended flag before IME."""
        if msg not in (0x0100, 0x0104):  # WM_KEYDOWN / WM_SYSKEYDOWN
            return True

        # data is KBDLLHOOKSTRUCT (passed as ctypes struct by value)
        vk = int(data.vkCode)
        if vk == 0 or vk == 0xE5:
            return True

        is_ext = bool(int(data.flags) & 0x01)
        sc = int(data.scanCode)
        cb = _capture_callback
        if cb:
            if tk_root:
                tk_root.after(0, lambda v=vk, e=is_ext, s=sc: cb(v, e, s))
            else:
                cb(vk, is_ext, sc)
        return False  # suppress — don't pass to on_press

    _listener = keyboard.Listener(win32_event_filter=on_win32_event, suppress=False)
    _listener.start()
    log.info("Keyboard listener started")


def stop_key_capture():
    global _listener, _capture_callback
    _capture_callback = None
    if _listener:
        _listener.stop()
        _listener = None
    log.info("Keyboard listener stopped")


def map_modifier_vk(vk: int, is_extended: bool) -> int:
    """Map generic modifier VK → specific left/right VK."""
    if vk == 0x11:  # Ctrl
        return VK_RCtrl if is_extended else VK_LCtrl
    if vk == 0x10:  # Shift
        return VK_RShift if is_extended else VK_LShift
    if vk == 0x12:  # Alt
        return VK_RAlt if is_extended else VK_LAlt
    return vk


def key_down(vk: int, modifier: int = 0):
    """Press key + modifiers down."""
    try:
        if modifier & 0x01: user32.keybd_event(VK_LCtrl,  0, KEYEVENTF_KEYDOWN, 0)
        if modifier & 0x02: user32.keybd_event(VK_LShift, 0, KEYEVENTF_KEYDOWN, 0)
        if modifier & 0x04: user32.keybd_event(VK_LAlt,   0, KEYEVENTF_KEYDOWN, 0)
        if modifier & 0x08: user32.keybd_event(VK_LWin,   0, KEYEVENTF_KEYDOWN, 0)
        if modifier & 0x10: user32.keybd_event(VK_RCtrl,  0, KEYEVENTF_KEYDOWN, 0)
        if modifier & 0x20: user32.keybd_event(VK_RShift, 0, KEYEVENTF_KEYDOWN, 0)
        if modifier & 0x40: user32.keybd_event(VK_RAlt,   0, KEYEVENTF_KEYDOWN, 0)
        if modifier & 0x80: user32.keybd_event(VK_RWin,   0, KEYEVENTF_KEYDOWN, 0)
        user32.keybd_event(vk, 0, KEYEVENTF_KEYDOWN, 0)
    except Exception as e:
        log.error(f"key_down error: {e}")


def key_up(vk: int, modifier: int = 0):
    """Release key + modifiers."""
    try:
        user32.keybd_event(vk, 0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x80: user32.keybd_event(VK_RWin,   0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x40: user32.keybd_event(VK_RAlt,   0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x20: user32.keybd_event(VK_RShift, 0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x10: user32.keybd_event(VK_RCtrl,  0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x08: user32.keybd_event(VK_LWin,   0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x04: user32.keybd_event(VK_LAlt,   0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x02: user32.keybd_event(VK_LShift, 0, KEYEVENTF_KEYUP, 0)
        if modifier & 0x01: user32.keybd_event(VK_LCtrl,  0, KEYEVENTF_KEYUP, 0)
    except Exception as e:
        log.error(f"key_up error: {e}")


# ── Key name map ────────────────────────────────────────────────
_KEY_NAMES = {
    0x08: "Backspace", 0x09: "Tab", 0x0D: "Enter", 0x10: "Shift",
    0x11: "Ctrl", 0x12: "Alt", 0x13: "Pause", 0x14: "CapsLock",
    0x1B: "Escape", 0x20: "Space", 0x21: "PageUp", 0x22: "PageDown",
    0x23: "End", 0x24: "Home", 0x25: "Left", 0x26: "Up",
    0x27: "Right", 0x28: "Down", 0x2C: "PrintScreen", 0x2D: "Insert",
    0x2E: "Delete",
    0x30: "0", 0x31: "1", 0x32: "2", 0x33: "3", 0x34: "4",
    0x35: "5", 0x36: "6", 0x37: "7", 0x38: "8", 0x39: "9",
    0x41: "A", 0x42: "B", 0x43: "C", 0x44: "D", 0x45: "E",
    0x46: "F", 0x47: "G", 0x48: "H", 0x49: "I", 0x4A: "J",
    0x4B: "K", 0x4C: "L", 0x4D: "M", 0x4E: "N", 0x4F: "O",
    0x50: "P", 0x51: "Q", 0x52: "R", 0x53: "S", 0x54: "T",
    0x55: "U", 0x56: "V", 0x57: "W", 0x58: "X", 0x59: "Y",
    0x5A: "Z", 0x5B: "LWin", 0x5C: "RWin",
    0x70: "F1", 0x71: "F2", 0x72: "F3", 0x73: "F4",
    0x74: "F5", 0x75: "F6", 0x76: "F7", 0x77: "F8",
    0x78: "F9", 0x79: "F10", 0x7A: "F11", 0x7B: "F12",
    0xA0: "LShift", 0xA1: "RShift", 0xA2: "LCtrl",
    0xA3: "RCtrl", 0xA4: "LAlt", 0xA5: "RAlt",
}


def vk_name(vk: int) -> str:
    return _KEY_NAMES.get(vk, f"VK_{vk:02X}")


_MOD_NAMES = [
    (0x01, "LCtrl"), (0x02, "LShift"), (0x04, "LAlt"), (0x08, "LWin"),
    (0x10, "RCtrl"), (0x20, "RShift"), (0x40, "RAlt"), (0x80, "RWin"),
]


def build_display(vk: int, modifier: int) -> str:
    """Build display string like 'LCtrl+Tab'."""
    parts = [name for mask, name in _MOD_NAMES if modifier & mask]
    parts.append(vk_name(vk))
    return "+".join(parts)
