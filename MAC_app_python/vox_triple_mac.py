#!/usr/bin/env python3
"""VoxTriple — ESP32 BT Microphone Config (macOS)

Shares ble_client.py and config_service.py with the Windows version.
Keyboard I/O is Mac-specific (keyboard_io_mac.py).
Requires Accessibility permission in System Settings for keyboard capture.
"""
import sys, os, asyncio, logging, tkinter as tk
from tkinter import ttk

# Import shared modules from sibling windows_app_python directory
_shared = os.path.join(os.path.dirname(__file__), "..", "windows_app_python")
if os.path.isdir(_shared):
    sys.path.insert(0, os.path.abspath(_shared))

import ble_client
import config_service
import keyboard_io_mac as keyboard_io

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(name)s] %(message)s")
log = logging.getLogger("VoxTriple")


# ── Async helpers ─────────────────────────────────────────────────
_loop: asyncio.AbstractEventLoop | None = None


def _run_async(coro):
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


# ── Modifier helpers ──────────────────────────────────────────────
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


# ── Main Application ──────────────────────────────────────────────
class VoxTripleApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("VoxTriple — ESP32 BT Mic Config (macOS)")
        self.root.geometry("620x820")
        self.root.minsize(580, 780)

        self.ble = ble_client.BleClient()
        self.ble.on_button_event = self._on_button_event
        self.ble.on_status = self._on_status

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

        self._tx_power = self._cfg.get("tx_power", 4)
        self._sleep_mode = tk.BooleanVar(value=self._cfg.get("sleep_mode", True))

        self._status_text = tk.StringVar(value="Searching for device...")
        self._last_event_text = tk.StringVar(value="None")
        self._connected = False

        self._build_ui()

        # Auto-connect on startup if previously paired
        if self._cfg.get("ble_address", 0):
            root.after(500, lambda: _run_async(self._auto_connect()))

    # ── UI construction ───────────────────────────────────────────
    def _build_ui(self):
        pad = {"padx": 4, "pady": 2}

        # Connection row
        conn_frame = ttk.LabelFrame(self.root, text="Connection", padding=8)
        conn_frame.pack(fill="x", padx=8, pady=4)

        btn_frame = ttk.Frame(conn_frame)
        btn_frame.pack(fill="x")
        ttk.Button(btn_frame, text="Pair New", command=self._scan_connect).pack(side="left", **pad)
        ttk.Button(btn_frame, text="Disconnect", command=self._disconnect).pack(side="left", **pad)
        ttk.Label(conn_frame, textvariable=self._status_text, foreground="gray").pack(anchor="w", **pad)

        status_sub = ttk.Frame(conn_frame)
        status_sub.pack(fill="x")
        self._hfp_label = ttk.Label(status_sub, text="HFP: --", font=("", 9, "bold"))
        self._hfp_label.pack(side="left", padx=8)
        self._audio_label = ttk.Label(status_sub, text="Audio: --", font=("", 9, "bold"))
        self._audio_label.pack(side="left", padx=8)

        # 4 button groups
        for i in range(4):
            self._build_button_group(i)

        # Last event
        evt_frame = ttk.LabelFrame(self.root, text="Last Button Event", padding=8)
        evt_frame.pack(fill="x", padx=8, pady=4)
        ttk.Label(evt_frame, textvariable=self._last_event_text, font=("", 12, "bold")).pack()

        # Device Settings
        dev_frame = ttk.LabelFrame(self.root, text="Device Settings", padding=8)
        dev_frame.pack(fill="x", padx=8, pady=4)
        dev_row = ttk.Frame(dev_frame)
        dev_row.pack(fill="x")
        ttk.Label(dev_row, text="TX Power:", font=("", 9, "bold")).pack(side="left", padx=4)
        self._tx_combo = ttk.Combobox(dev_row, width=18,
                                values=["0: -12 dBm (min)", "1: -9 dBm", "2: -6 dBm",
                                        "3: -3 dBm", "4: 0 dBm", "5: +3 dBm",
                                        "6: +6 dBm", "7: +9 dBm (max)"],
                                state="readonly")
        self._tx_combo.current(self._tx_power)
        self._tx_combo.bind("<<ComboboxSelected>>", self._on_tx_power_change)
        self._tx_combo.pack(side="left", padx=4)
        ttk.Label(dev_row, text="Sleep Mode:", font=("", 9, "bold")).pack(side="left", padx=(16, 4))
        ttk.Checkbutton(dev_row, text="Enabled", variable=self._sleep_mode).pack(side="left")

        # Action buttons
        act_frame = ttk.Frame(self.root)
        act_frame.pack(pady=8)
        ttk.Button(act_frame, text="Write to Device", command=self._write_device).pack(side="left", **pad)
        ttk.Button(act_frame, text="Save to File", command=self._save_file).pack(side="left", **pad)
        ttk.Button(act_frame, text="Load from File", command=self._load_file).pack(side="left", **pad)

        # Info
        info = ttk.LabelFrame(self.root, text="Info", padding=8)
        info.pack(fill="both", expand=True, padx=8, pady=4)
        msg = ("BLE Service: 0x1820 | Device: ESP32_BT_MIC\n"
               "Click 'Capture Key' then press any key to assign it.\n"
               "Modifier checkboxes apply on Write to Device.\n"
               "Hold Button 1 on ESP32 to PTT (push-to-talk).\n\n"
               "Keyboard capture requires Accessibility permission.\n"
               "System Settings → Privacy & Security → Accessibility")
        ttk.Label(info, text=msg, foreground="gray", font=("", 9)).pack(anchor="w")

        # Mac: Quit properly via Cmd+Q / menu
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        self.root.createcommand("tkAboutDialog", lambda: None)  # suppress default About

    def _build_button_group(self, i: int):
        b = self._btn[i]
        group = ttk.LabelFrame(self.root, text=f"Button {i+1}", padding=8)
        group.pack(fill="x", padx=8, pady=2)

        key_row = ttk.Frame(group)
        key_row.pack(fill="x")
        ttk.Label(key_row, text="Key:", width=5, font=("", 9, "bold")).pack(side="left")
        ttk.Label(key_row, textvariable=b["display"], width=18, relief="sunken", background="#f0f0f0").pack(side="left", padx=4)
        self._cap_btns = getattr(self, "_cap_btns", [None, None, None, None])
        btn = ttk.Button(key_row, text="Capture Key",
                         command=lambda idx=i: self._begin_capture(idx))
        btn.pack(side="left", padx=4)
        if not hasattr(self, "_cap_btns"):
            self._cap_btns = [None, None, None, None]
        self._cap_btns[i] = btn

        mod_frame = ttk.Frame(group)
        mod_frame.pack(fill="x", pady=2)
        ttk.Label(mod_frame, text="Modifiers:", font=("", 9, "bold")).pack(side="left")
        for mk, mask in _MOD_MASKS.items():
            label = {"lc": "LCtrl", "ls": "LShift", "la": "LOption", "lw": "LCmd",
                     "rc": "RCtrl", "rs": "RShift", "ra": "ROption", "rw": "RCmd"}[mk]
            ttk.Checkbutton(mod_frame, text=label, variable=b["mod_vars"][mk]).pack(side="left", padx=2)

    # ── Display update ────────────────────────────────────────────
    def _update_display(self, idx: int):
        b = self._btn[idx]
        mod = _build_modifier(b["mod_vars"])
        b["display"].set(keyboard_io.build_display(b["vk"].get(), mod))

    # ── Key capture ───────────────────────────────────────────────
    def _begin_capture(self, idx: int):
        for i, b in enumerate(self._btn):
            b["capturing"] = (i == idx)
        self._status_text.set(f"Capturing key for Button {idx+1}… Press a key.")

        def on_key(vk: int, _ext, _sc):
            if 0 <= idx < 4:
                self._btn[idx]["vk"].set(vk)
                self._update_display(idx)
            self._status_text.set("Key captured.")
            keyboard_io.stop_key_capture()

        keyboard_io.start_key_capture(on_key, self.root)

    def _on_tx_power_change(self, event):
        self._tx_power = self._tx_combo.current()
        if self._tx_power < 0:
            self._tx_power = 4

    # ── BLE operations ────────────────────────────────────────────
    def _scan_connect(self):
        _run_async(self._do_scan_connect())

    async def _do_scan_connect(self):
        self._status_text.set("Scanning...")
        addr = await ble_client.BleClient.scan()
        if not addr:
            self._status_text.set("ESP32_BT_MIC not found.")
            return
        self._status_text.set("Connecting...")
        ok = await self.ble.connect(addr)
        if ok:
            self._connected = True
            self._cfg["ble_address"] = int(addr.replace(":", ""), 16)
            self._cfg["device_address"] = addr
            config_service.save(self._cfg)
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
        addr = self.ble.address
        prev = f"Connected: {addr}" if addr else "Ready."
        self.root.after(3000, lambda p=prev: self._status_text.set(p))

    def _save_file(self):
        for i in range(4):
            key = f"button{i+1}"
            self._cfg[key] = {
                "vk_code": self._btn[i]["vk"].get(),
                "modifier": _build_modifier(self._btn[i]["mod_vars"]),
            }
        self._cfg["tx_power"] = self._tx_power
        self._cfg["sleep_mode"] = self._sleep_mode.get()
        config_service.save(self._cfg)
        self._status_text.set("Configuration saved to file.")

    def _load_file(self):
        self._cfg = config_service.load()
        for i in range(4):
            key = f"button{i+1}"
            b = self._cfg.get(key, {"vk_code": 0x0D, "modifier": 0})
            self._btn[i]["vk"].set(b["vk_code"])
            for mk, mask in _MOD_MASKS.items():
                self._btn[i]["mod_vars"][mk].set(bool(b["modifier"] & mask))
            self._update_display(i)
        self._tx_power = self._cfg.get("tx_power", 4)
        self._tx_combo.current(self._tx_power)
        self._sleep_mode.set(self._cfg.get("sleep_mode", True))
        self._status_text.set("Configuration loaded from file.")

    # ── Button event (from BLE) ───────────────────────────────────
    def _on_button_event(self, btn_id: int, state: int):
        self.root.after(0, lambda: self._handle_button_event(btn_id, state))

    def _handle_button_event(self, btn_id: int, state: int):
        s = "PRESSED" if state == 1 else "RELEASED"
        self._last_event_text.set(f"Button {btn_id + 1} {s}")
        # Key input is handled by ESP32 BLE HID directly — no Python simulation needed.

    def _on_status(self, hfp: int, audio: int):
        self.root.after(0, lambda: self._handle_status(hfp, audio))

    def _handle_status(self, hfp: int, audio: int):
        hfp_s = "Connected" if hfp else "--"
        audio_s = "Active" if audio else "--"
        self._hfp_label.configure(text=f"HFP: {hfp_s}")
        self._audio_label.configure(text=f"Audio: {audio_s}")

    def _on_close(self):
        keyboard_io.stop_key_capture()
        _run_async(self.ble.disconnect())
        stop_asyncio_loop()
        self.root.destroy()


# ── Entry point ───────────────────────────────────────────────────
def main():
    root = tk.Tk()
    VoxTripleApp(root)

    # Start asyncio loop in a background thread
    import threading
    t = threading.Thread(target=start_asyncio_loop, daemon=True)
    t.start()

    try:
        root.mainloop()
    except KeyboardInterrupt:
        pass
    finally:
        stop_asyncio_loop()


if __name__ == "__main__":
    main()
