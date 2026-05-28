"""Mac keyboard capture and simulation via pynput.

Capture: pynput.keyboard.Listener with on_press callback.
On macOS this requires Accessibility permission (System Settings →
Privacy & Security → Accessibility).

Simulation: pynput.keyboard.Controller via Quartz/CGEvent.
Windows VK codes from ESP32 are translated to Mac key events.
"""
import logging
from pynput import keyboard
from pynput.keyboard import Key, KeyCode

log = logging.getLogger(__name__)

# ── Simulation: Windows VK → pynput Key ───────────────────────────
_controller = keyboard.Controller()

# Modifier VK → Mac modifier keys
_MOD_VK_TO_KEY = {
    0xA2: Key.ctrl_l,   # VK_LCtrl
    0xA3: Key.ctrl_r,   # VK_RCtrl
    0xA0: Key.shift_l,  # VK_LShift
    0xA1: Key.shift_r,  # VK_RShift
    0xA4: Key.alt_l,    # VK_LAlt   → Option (left)
    0xA5: Key.alt_r,    # VK_RAlt   → Option (right)
    0x5B: Key.cmd_l,    # VK_LWin   → Command (left)
    0x5C: Key.cmd_r,    # VK_RWin   → Command (right)
}

# Standard VK → pynput Key (non-printable special keys)
_VK_TO_KEY = {
    0x08: Key.backspace, 0x09: Key.tab,      0x0D: Key.enter,
    0x1B: Key.esc,       0x20: Key.space,    0x2E: Key.delete,
    0x21: Key.page_up,   0x22: Key.page_down, 0x23: Key.end,
    0x24: Key.home,      0x25: Key.left,     0x26: Key.up,
    0x27: Key.right,     0x28: Key.down,     0x2D: Key.insert,
    0x2C: Key.print_screen, 0x13: Key.pause,
    0x14: Key.caps_lock, 0x90: Key.num_lock,
    0x6A: KeyCode.from_char("*"), 0x6B: KeyCode.from_char("+"),
    0x6D: KeyCode.from_char("-"), 0x6E: KeyCode.from_char("."),
    0x6F: KeyCode.from_char("/"),
    0x70: Key.f1,  0x71: Key.f2,  0x72: Key.f3,  0x73: Key.f4,
    0x74: Key.f5,  0x75: Key.f6,  0x76: Key.f7,  0x77: Key.f8,
    0x78: Key.f9,  0x79: Key.f10, 0x7A: Key.f11, 0x7B: Key.f12,
}


def _vk_to_key(vk: int):
    """Convert Windows VK code to pynput Key/KeyCode."""
    # Check special key table first
    if vk in _VK_TO_KEY:
        return _VK_TO_KEY[vk]
    # Printable ASCII characters (0x30-0x5A, etc.)
    if 0x20 <= vk <= 0x7E:
        return KeyCode.from_char(chr(vk))
    return None


def key_down(vk: int, modifier: int = 0):
    """Press key + modifiers on macOS."""
    try:
        # Press modifiers first (order: ctrl, shift, alt, cmd)
        if modifier & 0x01: _controller.press(Key.ctrl_l)
        if modifier & 0x02: _controller.press(Key.shift_l)
        if modifier & 0x04: _controller.press(Key.alt_l)
        if modifier & 0x08: _controller.press(Key.cmd_l)
        if modifier & 0x10: _controller.press(Key.ctrl_r)
        if modifier & 0x20: _controller.press(Key.shift_r)
        if modifier & 0x40: _controller.press(Key.alt_r)
        if modifier & 0x80: _controller.press(Key.cmd_r)
        key = _vk_to_key(vk)
        if key:
            _controller.press(key)
    except Exception as e:
        log.error(f"key_down error: {e}")


def key_up(vk: int, modifier: int = 0):
    """Release key + modifiers on macOS."""
    try:
        key = _vk_to_key(vk)
        if key:
            _controller.release(key)
        if modifier & 0x80: _controller.release(Key.cmd_r)
        if modifier & 0x40: _controller.release(Key.alt_r)
        if modifier & 0x20: _controller.release(Key.shift_r)
        if modifier & 0x10: _controller.release(Key.ctrl_r)
        if modifier & 0x08: _controller.release(Key.cmd_l)
        if modifier & 0x04: _controller.release(Key.alt_l)
        if modifier & 0x02: _controller.release(Key.shift_l)
        if modifier & 0x01: _controller.release(Key.ctrl_l)
    except Exception as e:
        log.error(f"key_up error: {e}")


# ── Capture: pynput Key → Windows VK ──────────────────────────────
_listener: keyboard.Listener | None = None
_capture_callback = None


# Reverse mapping: pynput Key → VK (for capture / key learning)
_KEY_TO_VK = {
    Key.backspace: 0x08, Key.tab: 0x09,       Key.enter: 0x0D,
    Key.esc: 0x1B,       Key.space: 0x20,     Key.delete: 0x2E,
    Key.page_up: 0x21,   Key.page_down: 0x22, Key.end: 0x23,
    Key.home: 0x24,      Key.left: 0x25,      Key.up: 0x26,
    Key.right: 0x27,     Key.down: 0x28,      Key.insert: 0x2D,
    Key.print_screen: 0x2C, Key.pause: 0x13,
    Key.caps_lock: 0x14, Key.num_lock: 0x90,
    Key.ctrl_l: 0xA2,    Key.ctrl_r: 0xA3,
    Key.shift_l: 0xA0,   Key.shift_r: 0xA1,
    Key.alt_l: 0xA4,     Key.alt_r: 0xA5,
    Key.cmd_l: 0x5B,     Key.cmd_r: 0x5C,
    Key.f1: 0x70,  Key.f2: 0x71,  Key.f3: 0x72,  Key.f4: 0x73,
    Key.f5: 0x74,  Key.f6: 0x75,  Key.f7: 0x76,  Key.f8: 0x77,
    Key.f9: 0x78,  Key.f10: 0x79, Key.f11: 0x7A, Key.f12: 0x7B,
}

# Additional Mac-specific key → VK
_KEY_TO_VK[Key.media_volume_mute] = 0xAD
_KEY_TO_VK[Key.media_volume_down] = 0xAE
_KEY_TO_VK[Key.media_volume_up] = 0xAF
_KEY_TO_VK[Key.media_play_pause] = 0xB3
_KEY_TO_VK[Key.media_next] = 0xB0
_KEY_TO_VK[Key.media_previous] = 0xB1


def _key_to_vk(key) -> int | None:
    """Convert pynput Key/KeyCode to Windows VK code."""
    if isinstance(key, Key):
        return _KEY_TO_VK.get(key)
    if isinstance(key, KeyCode):
        # Try .vk attribute first (may be None on macOS)
        if key.vk is not None:
            return key.vk
        # Try character mapping
        if key.char and len(key.char) == 1:
            return ord(key.char.upper())
    return None


def start_key_capture(callback, tk_root=None):
    """Start keyboard listener for capture mode.
    `callback(vk, is_extended, scan_code)` — is_extended and scan_code
    are always 0 on macOS (not applicable)."""
    global _listener, _capture_callback
    _capture_callback = callback

    def on_press(key):
        try:
            # Ignore modifier-only presses during capture
            if isinstance(key, Key) and key in (
                Key.ctrl, Key.ctrl_l, Key.ctrl_r, Key.shift, Key.shift_l,
                Key.shift_r, Key.alt, Key.alt_l, Key.alt_r, Key.cmd,
                Key.cmd_l, Key.cmd_r
            ):
                return True
        except Exception:
            pass

        vk = _key_to_vk(key)
        cb = _capture_callback
        if vk and cb:
            if tk_root:
                tk_root.after(0, lambda v=vk: cb(v, False, 0))
            else:
                cb(vk, False, 0)
        # Stop listener after first captured key
        return False

    _listener = keyboard.Listener(on_press=on_press)
    _listener.start()
    log.info("Keyboard listener started (macOS)")


def stop_key_capture():
    global _listener, _capture_callback
    _capture_callback = None
    if _listener:
        _listener.stop()
        _listener = None
    log.info("Keyboard listener stopped")


# ── Modifier mapping (same as Windows version) ────────────────────
def map_modifier_vk(vk: int, is_extended: bool) -> int:
    """Map generic modifier VK to specific left/right VK."""
    if vk == 0x11:  # Ctrl
        return 0xA3 if is_extended else 0xA2  # VK_RCtrl : VK_LCtrl
    if vk == 0x10:  # Shift
        return 0xA1 if is_extended else 0xA0  # VK_RShift : VK_LShift
    if vk == 0x12:  # Alt
        return 0xA5 if is_extended else 0xA4  # VK_RAlt : VK_LAlt
    return vk


# ── Key name map (shared with Windows version) ────────────────────
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
    0x5A: "Z", 0x5B: "LCmd", 0x5C: "RCmd",
    0x70: "F1", 0x71: "F2", 0x72: "F3", 0x73: "F4",
    0x74: "F5", 0x75: "F6", 0x76: "F7", 0x77: "F8",
    0x78: "F9", 0x79: "F10", 0x7A: "F11", 0x7B: "F12",
    0xA0: "LShift", 0xA1: "RShift", 0xA2: "LCtrl",
    0xA3: "RCtrl", 0xA4: "LOption", 0xA5: "ROption",
}


def vk_name(vk: int) -> str:
    return _KEY_NAMES.get(vk, f"VK_{vk:02X}")


_MOD_NAMES = [
    (0x01, "LCtrl"), (0x02, "LShift"), (0x04, "LOption"), (0x08, "LCmd"),
    (0x10, "RCtrl"), (0x20, "RShift"), (0x40, "ROption"), (0x80, "RCmd"),
]


def build_display(vk: int, modifier: int) -> str:
    """Build display string like 'LCtrl+Tab'."""
    parts = [name for mask, name in _MOD_NAMES if modifier & mask]
    parts.append(vk_name(vk))
    return "+".join(parts)
