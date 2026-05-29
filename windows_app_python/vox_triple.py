#!/usr/bin/env python3
"""VoxTriple — ESP32 Bluetooth PTT Microphone Config (Python Edition).

tkinter GUI + bleak BLE + Win32 keybd_event.
Same functionality as the C# WPF version:
  - BLE scan & connect to ESP32_BT_MIC
  - Capture keyboard keys (Win32 low-level hook, bypasses IME)
  - Modifier checkboxes (Ctrl / Shift / Alt / Win)
  - Write button mappings to ESP32 over BLE
  - Real-time button event display
  - Keyboard simulation via keybd_event
  - Auto-start on Windows boot (shortcut in Startup folder)
  - JSON config persistence
"""
import asyncio
import json
import logging
import os
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox
from concurrent.futures import ThreadPoolExecutor

import ble_client
import keyboard_io
import config_service

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(name)s] %(message)s")

# ── Asyncio bridge ──────────────────────────────────────────────
_loop: asyncio.AbstractEventLoop | None = None
_executor = ThreadPoolExecutor(max_workers=2)


def _run_async(coro):
    """Schedule a coroutine on the asyncio event loop and return a Future."""
    if _loop is None:
        return None
    return asyncio.run_coroutine_threadsafe(coro, _loop)


def start_asyncio_loop():
    global _loop
    _loop = asyncio.new_event_loop()
    asyncio.set_event_loop(_loop)
    _loop.run_forever()


def stop_asyncio_loop():
    if _loop:
        _loop.call_soon_threadsafe(_loop.stop)


# ── Modifier helpers ────────────────────────────────────────────
_MOD_MASKS = {
    "lc": 0x01, "ls": 0x02, "la": 0x04, "lw": 0x08,
    "rc": 0x10, "rs": 0x20, "ra": 0x40, "rw": 0x80,
}


def _build_modifier(vars: dict) -> int:
    mod = 0
    for key, mask in _MOD_MASKS.items():
        if vars.get(key, tk.BooleanVar()).get():
            mod |= mask
    return mod


# ── Main App ────────────────────────────────────────────────────
class VoxTripleApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("VoxTriple — ESP32 BT Mic Config (Python)")
        root.geometry("620x780")
        root.minsize(580, 740)
        root.resizable(True, True)

        self.ble = ble_client.BleClient()
        self.ble.on_button_event = self._on_button_event
        self.ble.on_status = self._on_status
        self._capture_idx = -1  # -1 = none, 0/1/2 = active

        # Config
        self._cfg = config_service.load()

        # Button state
        self._btn = []
        for i in range(4):
            key = f"button{i+1}"
            b = self._cfg.get(key, {"vk_code": 0x0D, "modifier": 0})
            self._btn.append({
                "vk": tk.IntVar(value=b["vk_code"]),
                "mod_vars": {},
                "capturing": False,
                "display": tk.StringVar(value=keyboard_io.build_display(b["vk_code"], b["modifier"])),
            })
            for mk in _MOD_MASKS:
                self._btn[i]["mod_vars"][mk] = tk.BooleanVar(value=bool(b["modifier"] & _MOD_MASKS[mk]))
                self._btn[i]["mod_vars"][mk].trace_add("write", lambda *a, idx=i: self._update_display(idx))

        self._tx_power = self._cfg.get("tx_power", 4)  # plain int, updated via combobox
        self._sleep_mode = tk.BooleanVar(value=self._cfg.get("sleep_mode", True))

        self._status_text = tk.StringVar(value="Searching for device...")
        self._last_event_text = tk.StringVar(value="None")
        self._connected = False

        self._build_ui()

        # Auto-connect on startup if previously paired
        if self._cfg.get("ble_address", 0):
            root.after(500, lambda: _run_async(self._auto_connect()))

    # ── UI ──────────────────────────────────────────────────────
    def _build_ui(self):
        pad = {"padx": 4, "pady": 2}

        # Connection status (no buttons — auto-connect handles everything)
        conn_frame = ttk.LabelFrame(self.root, text="Connection / 连接状态", padding=8)
        conn_frame.pack(fill="x", padx=8, pady=4)
        ttk.Label(conn_frame, textvariable=self._status_text, foreground="gray").pack(anchor="w", **pad)
        status_sub = ttk.Frame(conn_frame)
        status_sub.pack(fill="x")
        self._hfp_label = ttk.Label(status_sub, text="HFP: --", font=("", 9, "bold"))
        self._hfp_label.pack(side="left", padx=8)
        self._audio_label = ttk.Label(status_sub, text="Audio: --", font=("", 9, "bold"))
        self._audio_label.pack(side="left", padx=8)

        # Last button event (moved up, next to connection)
        evt_frame = ttk.LabelFrame(self.root, text="Last Button Event / 最近按钮事件", padding=8)
        evt_frame.pack(fill="x", padx=8, pady=4)
        ttk.Label(evt_frame, textvariable=self._last_event_text, font=("", 12, "bold")).pack()

        # 4 button groups
        for i in range(4):
            self._build_button_group(i)

        # Device Settings
        dev_frame = ttk.LabelFrame(self.root, text="Device Settings / 设备设置", padding=8)
        dev_frame.pack(fill="x", padx=8, pady=4)
        dev_row = ttk.Frame(dev_frame)
        dev_row.pack(fill="x")
        ttk.Label(dev_row, text="TX Power / 发射功率:", font=("", 9, "bold")).pack(side="left", padx=4)
        self._tx_combo = ttk.Combobox(dev_row, width=18,
                                values=["0: -12 dBm (min)", "1: -9 dBm", "2: -6 dBm",
                                        "3: -3 dBm", "4: 0 dBm", "5: +3 dBm",
                                        "6: +6 dBm", "7: +9 dBm (max)"],
                                state="readonly")
        self._tx_combo.current(self._tx_power)
        self._tx_combo.bind("<<ComboboxSelected>>", self._on_tx_power_change)
        self._tx_combo.pack(side="left", padx=4)
        ttk.Label(dev_row, text="Sleep Mode / 睡眠模式:", font=("", 9, "bold")).pack(side="left", padx=(16, 4))
        ttk.Checkbutton(dev_row, text="Enabled / 启用", variable=self._sleep_mode).pack(side="left")

        # Action buttons — Write is the primary action, big and prominent
        act_frame = ttk.Frame(self.root)
        act_frame.pack(pady=8)
        ttk.Button(act_frame, text="Write to Keyboard / 写入到蓝牙键盘",
                   command=self._write_device).pack(side="left", padx=4, ipadx=20, ipady=4)

        # Info (simplified)
        info = ttk.LabelFrame(self.root, text="Info / 说明", padding=8)
        info.pack(fill="both", expand=True, padx=8, pady=4)
        msg = ("Click 'Capture Key' then press a key to assign it.\n"
               "Modifier checkboxes apply on Write to Keyboard.\n"
               "The keyboard works without this app — open only to change settings.\n\n"
               "点击「Capture Key」然后按键来捕获。\n"
               "修饰键勾选框在「写入到蓝牙键盘」时生效。\n"
               "蓝牙键盘独立工作 — 此应用仅用于修改配置。")
        ttk.Label(info, text=msg, foreground="gray", font=("", 9)).pack(anchor="w")

        # Quit cleanup
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_button_group(self, i: int):
        b = self._btn[i]
        group = ttk.LabelFrame(self.root, text=f"Button {i+1}", padding=8)
        group.pack(fill="x", padx=8, pady=2)

        key_row = ttk.Frame(group)
        key_row.pack(fill="x")
        ttk.Label(key_row, text="Key:", width=5, font=("", 9, "bold")).pack(side="left")
        ttk.Label(key_row, textvariable=b["display"], width=18, relief="sunken", background="#f0f0f0").pack(side="left", padx=4)
        self._cap_btns = getattr(self, "_cap_btns", [None, None, None, None])
        btn = ttk.Button(key_row, text="Capture Key / 捕获按键",
                         command=lambda idx=i: self._begin_capture(idx))
        btn.pack(side="left", padx=4)
        if not hasattr(self, "_cap_btns"):
            self._cap_btns = [None, None, None, None]
        self._cap_btns[i] = btn

        mod_frame = ttk.Frame(group)
        mod_frame.pack(fill="x", pady=2)
        ttk.Label(mod_frame, text="Modifiers:", font=("", 9, "bold")).pack(side="left")
        for mk, mask in _MOD_MASKS.items():
            label = {"lc": "LCtrl", "ls": "LShift", "la": "LAlt", "lw": "LWin",
                     "rc": "RCtrl", "rs": "RShift", "ra": "RAlt", "rw": "RWin"}[mk]
            ttk.Checkbutton(mod_frame, text=label, variable=b["mod_vars"][mk]).pack(side="left", padx=2)

    # ── Display update ───────────────────────────────────────────
    def _update_display(self, idx: int):
        b = self._btn[idx]
        mod = _build_modifier(b["mod_vars"])
        b["display"].set(keyboard_io.build_display(b["vk"].get(), mod))

    # ── BLE actions ──────────────────────────────────────────────
    def _scan_connect(self):
        self._status_text.set("Scanning…")
        _run_async(self._do_scan_connect())

    async def _do_scan_connect(self):
        addr = await ble_client.BleClient.scan()
        if not addr:
            self._status_text.set("ESP32_BT_MIC not found. Is it powered on?")
            return
        self._status_text.set("Connecting…")
        ok = await self.ble.connect(addr)
        if ok:
            self._connected = True
            self._cfg["ble_address"] = int(addr.replace(":", ""), 16)
            self._cfg["device_address"] = addr
            config_service.save(self._cfg)
            self._status_text.set(f"Connected: {addr}")
            # Read mappings
            for i in range(4):
                r = await self.ble.read_button_mapping(i)
                if r:
                    self._btn[i]["vk"].set(r[0])
                    for mk, mask in _MOD_MASKS.items():
                        self._btn[i]["mod_vars"][mk].set(bool(r[1] & mask))
                    self._update_display(i)
            # Read TX power and sleep mode
            tx = await self.ble.read_tx_power()
            if tx is not None:
                self._tx_power = tx
                self._tx_combo.current(tx)
            sl = await self.ble.read_sleep_mode()
            if sl is not None:
                self._sleep_mode.set(bool(sl))
        else:
            self._status_text.set("Connection failed.")

    async def _auto_connect(self):
        addr = await ble_client.BleClient.scan(5.0)
        if addr:
            ok = await self.ble.connect(addr)
            if ok:
                self._connected = True
                self._status_text.set(f"Connected: {addr}")
                for i in range(4):
                    r = await self.ble.read_button_mapping(i)
                    if r:
                        self._btn[i]["vk"].set(r[0])
                        for mk, mask in _MOD_MASKS.items():
                            self._btn[i]["mod_vars"][mk].set(bool(r[1] & mask))
                        self._update_display(i)
                tx = await self.ble.read_tx_power()
                if tx is not None:
                    self._tx_power = tx
                    self._tx_combo.current(tx)
                sl = await self.ble.read_sleep_mode()
                if sl is not None:
                    self._sleep_mode.set(bool(sl))
                return
        self._status_text.set("Device not found. Click 'Pair New' or check power.")

    def _disconnect(self):
        _run_async(self.ble.disconnect())
        self._connected = False
        self._status_text.set("Disconnected.")

    def _write_device(self):
        if not self._connected:
            self._status_text.set("BLE not connected.")
            return
        _run_async(self._do_write_device())

    async def _do_write_device(self):
        for i in range(4):
            vk = self._btn[i]["vk"].get()
            mod = _build_modifier(self._btn[i]["mod_vars"])
            ok = await self.ble.write_button_mapping(i, vk, mod)
            if not ok:
                self._status_text.set(f"Write btn{i+1} failed.")
                return
        ok_tx = await self.ble.write_tx_power(self._tx_power)
        if not ok_tx:
            self._status_text.set("Write TX power failed.")
            return
        ok_sl = await self.ble.write_sleep_mode(1 if self._sleep_mode.get() else 0)
        if not ok_sl:
            self._status_text.set("Write sleep mode failed.")
            return
        self._status_text.set("Settings written to device.")
        # Reset status after 3 seconds — use saved text, not live _connected
        addr = self.ble.address
        prev = f"Connected: {addr}" if addr else "Ready."
        self.root.after(3000, lambda p=prev: self._status_text.set(p))

    # ── Key capture ─────────────────────────────────────────────
    def _begin_capture(self, idx: int):
        self._capture_idx = idx
        self._btn[idx]["capturing"] = True
        self._cap_btns[idx].configure(text="Capturing… press any key / 按任意键…")
        keyboard_io.start_key_capture(self._on_key_captured, tk_root=self.root)

    def _on_key_captured(self, vk: int, is_extended: bool, scan_code: int):
        idx = self._capture_idx
        if idx < 0:
            return
        keyboard_io.stop_key_capture()

        # Right Shift detection via scan code
        if vk == 0x10 and scan_code == keyboard_io.SCAN_RSHIFT:
            is_extended = True

        mapped = keyboard_io.map_modifier_vk(vk, is_extended)
        mod = _build_modifier(self._btn[idx]["mod_vars"])
        is_mod = mapped in (0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x5B, 0x5C)

        if is_mod:
            # Modifier-only capture: clear other modifiers, set VK
            self._btn[idx]["vk"].set(mapped)
            for mk in _MOD_MASKS:
                self._btn[idx]["mod_vars"][mk].set(False)
        else:
            self._btn[idx]["vk"].set(mapped)

        self._update_display(idx)
        self._btn[idx]["capturing"] = False
        self._capture_idx = -1
        self._cap_btns[idx].configure(text="Capture Key / 捕获按键")

    # ── BLE callbacks ────────────────────────────────────────────
    def _on_button_event(self, btn_id: int, state: int):
        """Called from BLE thread — dispatch to tkinter main thread."""
        self.root.after(0, lambda: self._handle_button_event(btn_id, state))

    def _handle_button_event(self, btn_id: int, state: int):
        s = "PRESSED" if state == 1 else "RELEASED"
        self._last_event_text.set(f"Button {btn_id + 1} {s}")
        # Key input is now handled by ESP32 BLE HID directly.
        # No need for Python to simulate keystrokes — avoids double input.

    def _on_status(self, hfp: int, audio: int):
        self.root.after(0, lambda: self._handle_status(hfp, audio))

    def _handle_status(self, hfp: int, audio: int):
        self._hfp_label.configure(text=f"HFP: {'Connected' if hfp else '--'}")
        self._audio_label.configure(text=f"Audio: {'Active' if audio else '--'}")

    # ── TX Power ─────────────────────────────────────────────────
    def _on_tx_power_change(self, event):
        """Update _tx_power int when combobox selection changes."""
        self._tx_power = self._tx_combo.current()
        if self._tx_power < 0:
            self._tx_power = 4

    # ── Close ───────────────────────────────────────────────────
    def _on_close(self):
        keyboard_io.stop_key_capture()
        _run_async(self.ble.disconnect())
        stop_asyncio_loop()
        self.root.destroy()


def main():
    # Start asyncio event loop in background thread
    t = threading.Thread(target=start_asyncio_loop, daemon=True)
    t.start()

    root = tk.Tk()
    VoxTripleApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
