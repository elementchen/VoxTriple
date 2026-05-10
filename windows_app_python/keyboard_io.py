"""Keyboard capture & simulation via Win32 APIs (ctypes).

Capture uses a low-level keyboard hook (WH_KEYBOARD_LL) to grab raw
VK codes before the IME can transform them — same approach as the C# WPF
version's WndProc hook.

Simulation uses keybd_event, proven reliable in the C# version.
"""
import ctypes
import ctypes.wintypes as w
import threading
import logging

log = logging.getLogger(__name__)

# ── Win32 constants ────────────────────────────────────────────
WH_KEYBOARD_LL = 13
WM_KEYDOWN     = 0x0100
WM_SYSKEYDOWN  = 0x0104
KEYEVENTF_KEYDOWN = 0x0000
KEYEVENTF_KEYUP   = 0x0002

# Modifier VK codes
VK_LCtrl  = 0xA2; VK_RCtrl  = 0xA3
VK_LShift = 0xA0; VK_RShift = 0xA1
VK_LAlt   = 0xA4; VK_RAlt   = 0xA5
VK_LWin   = 0x5B; VK_RWin   = 0x5C

# Scan code for Right Shift (no extended flag in Win32)
SCAN_RSHIFT = 0x36

# ── Structs ─────────────────────────────────────────────────────
class KBDLLHOOKSTRUCT(ctypes.Structure):
    _fields_ = [
        ("vkCode",      w.DWORD),
        ("scanCode",    w.DWORD),
        ("flags",       w.DWORD),
        ("time",        w.DWORD),
        ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong)),
    ]

HOOKPROC = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_int, w.WPARAM, ctypes.POINTER(KBDLLHOOKSTRUCT))
LowLevelKeyboardProc = HOOKPROC

# ── Win32 DLL imports ───────────────────────────────────────────
user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

user32.SetWindowsHookExW.restype = ctypes.c_void_p
user32.CallNextHookEx.restype = ctypes.c_long
user32.keybd_event.restype = None

# ── Globals ─────────────────────────────────────────────────────
_hook_handle = None
_capture_callback = None  # called with (vk_code: int, is_extended: bool, scan_code: int)


def _low_level_hook(nCode: int, wParam: int, lParam: ctypes.POINTER(KBDLLHOOKSTRUCT)) -> int:
    global _capture_callback
    if nCode >= 0 and _capture_callback:
        kb = lParam.contents
        if wParam in (WM_KEYDOWN, WM_SYSKEYDOWN):
            vk = kb.vkCode
            if vk not in (0, 0xE5) and not (kb.flags & 0x80):  # skip IME process & key-up flag
                is_ext = bool(kb.flags & 0x01)    # extended-key flag
                sc = kb.scanCode
                _capture_callback(vk, is_ext, sc)
    return user32.CallNextHookEx(0, nCode, wParam, lParam)


_hook_cb = LowLevelKeyboardProc(_low_level_hook)


def start_key_capture(callback):
    """Install global low-level keyboard hook. `callback(vk, is_extended, scan_code)`."""
    global _hook_handle, _capture_callback
    _capture_callback = callback
    _hook_handle = user32.SetWindowsHookExW(
        WH_KEYBOARD_LL, _hook_cb,
        kernel32.GetModuleHandleW(None), 0,
    )
    if not _hook_handle:
        log.error("Failed to install keyboard hook")


def stop_key_capture():
    global _hook_handle, _capture_callback
    _capture_callback = None
    if _hook_handle:
        user32.UnhookWindowsHookEx(_hook_handle)
        _hook_handle = None


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
