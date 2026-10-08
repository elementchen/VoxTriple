#!/usr/bin/env python3
from __future__ import annotations
"""VoxTriple — ESP32 BT Microphone Config (Windows - Webview Edition)

Shares spp_client.py with the macOS version.
Keystrokes are captured natively in Webview via JS to avoid system crashes.
Wired config and OTA firmware flashing are handled via physical USB Serial.
"""
import sys
import os
import asyncio
import logging
import urllib.request
import json
import threading
import webview
import serial.tools.list_ports

import spp_client

log_file = os.path.join(os.path.expanduser("~"), "VoxTriple.log")
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(message)s",
    handlers=[
        logging.FileHandler(log_file, encoding="utf-8", mode="a"),
        logging.StreamHandler(sys.stdout) if sys.stdout else logging.NullHandler()
    ]
)
log = logging.getLogger("VoxTriple")

# ── Async background loop ──────────────────────────────────────────
_loop: asyncio.AbstractEventLoop | None = None
_thread: threading.Thread | None = None

def start_asyncio_loop():
    global _loop
    _loop = asyncio.new_event_loop()
    asyncio.set_event_loop(_loop)
    _loop.run_forever()

def run_coro(coro):
    if _loop is None:
        return None
    return asyncio.run_coroutine_threadsafe(coro, _loop)

# ── Webview Python JS-Bridge API ───────────────────────────────────────
class Api:
    def __init__(self):
        self.spp = spp_client.SppClient()
        self.spp.on_button_event = self._on_button_event
        self.spp.on_status = self._on_status
        self._connected = False
        self._window = None
        self._github_version = None
        self._github_bin_url = None

    def set_window(self, window):
        self._window = window

    def get_ports(self) -> list[str]:
        """Scan Windows local for COM ports."""
        ports = []
        for p in serial.tools.list_ports.comports():
            ports.append(p.device)
        return ports

    def connect_device(self, port: str) -> bool:
        """Connect to physical USB-serial device."""
        log.info(f"Connecting to {port}...")
        future = run_coro(self.spp.connect(port))
        ok = future.result(timeout=6.0)
        self._connected = ok
        return ok

    def disconnect_device(self) -> bool:
        """Disconnect current device."""
        log.info("Disconnecting serial port...")
        future = run_coro(self.spp.disconnect())
        future.result(timeout=3.0)
        self._connected = False
        return True

    def fetch_config(self) -> dict | None:
        """Fetch config cache from the device."""
        if not self._connected:
            return None
            
        future = run_coro(self.spp._fetch_config())
        ok = future.result(timeout=3.0)
        if not ok:
            return None
            
        # Format mapping list to JSON compatible
        config = {
            "version": self.spp._config_cache.get("version", "1.0.9"),
            "tx_power": self.spp._config_cache.get("tx_power", 4),
            "sleep_mode": self.spp._config_cache.get("sleep_mode", 1),
            "mic_enabled": self.spp._config_cache.get("mic_enabled", 1),
            "board_model": self.spp._config_cache.get("board_model", 0),
            "board_name": self.spp._config_cache.get("board_name", "WEMOS 18650"),
            "sleep_timeout_min": self.spp._config_cache.get("sleep_timeout_min", 5),
            "mappings": []
        }
        for i in range(4):
            vk = self.spp._config_cache.get(f"btn{i+1}_vk", 0)
            mod = self.spp._config_cache.get(f"btn{i+1}_mod", 0)
            config["mappings"].append({"vk": vk, "mod": mod})
            
        return config

    def write_config(self, mappings: list, tx: int, sleep: int, mic: int) -> bool:
        """Write all parameters to board Flash at once."""
        if not self._connected:
            return False
            
        log.info(f"Save parameters: mapping={mappings}, tx={tx}, sleep={sleep}, mic={mic}")
        future = run_coro(self.spp.set_config(mappings, tx, sleep, mic))
        ok = future.result(timeout=5.0)
        return ok

    def set_board_model(self, model: int) -> dict:
        """Set board hardware model on device and trigger restart."""
        if not self._connected:
            return {"success": False, "message": "Not connected"}
        log.info(f"Set board hardware model: {model}")
        future = run_coro(self.spp.write_board_model(model))
        ok = future.result(timeout=5.0)
        return {"success": ok, "message": "ok" if ok else "write failed"}

    def set_sleep_timeout_min(self, minutes: int) -> dict:
        """Set inactivity sleep timeout (minutes) on device and trigger restart."""
        if not self._connected:
            return {"success": False, "message": "Not connected"}
        log.info(f"Set sleep timeout: {minutes} min")
        future = run_coro(self.spp.write_sleep_timeout_min(minutes))
        ok = future.result(timeout=5.0)
        return {"success": ok, "message": "ok" if ok else "write failed"}

    def reset_bt_pairing(self) -> dict:
        """Expose API to frontend to forget BT pairings on the device and reboot."""
        if not self.spp or not self.spp.is_connected:
            return {"status": "error", "message": "Device not connected"}
        future = run_coro(self.spp.reset_bt_pairing())
        try:
            ok = future.result(timeout=4.0)
            if ok:
                return {"status": "ok"}
            return {"status": "error", "message": "Command rejected"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def select_local_bin(self) -> str | None:
        """Select a local firmware file using Windows native file dialog via Tkinter fallback to ensure non-UI thread safety."""
        log.info("Opening Windows file selection dialog via Tkinter...")
        try:
            import tkinter as tk
            from tkinter import filedialog
            root = tk.Tk()
            root.withdraw()
            root.attributes("-topmost", True)
            file_path = filedialog.askopenfilename(
                title="Select VoxTriple firmware file (.bin)",
                filetypes=[("Firmware Files (*.bin)", "*.bin"), ("All Files", "*.*")]
            )
            root.destroy()
            if file_path:
                normalized_path = file_path.replace("\\", "/")
                log.info(f"User selected file via Tkinter: {normalized_path}")
                return normalized_path
        except Exception as e:
            log.error(f"Tkinter file dialog failed: {e}")
        return None

    def trigger_ota(self, bin_path: str) -> bool:
        """Upload firmware to ESP32 via serial link."""
        if not self._connected:
            return False
            
        log.info(f"Starting OTA update with file: {bin_path}")
        
        def progress_cb(written, total):
            if self._window:
                self._window.evaluate_js(f"onOtaProgress({written}, {total})")

        future = run_coro(self.spp.upload_firmware(bin_path, progress_cb))
        ok = future.result(timeout=60.0)
        return ok

    def check_update(self) -> dict:
        """Check GitHub latest update version and return results to JS."""
        future = run_coro(self._check_github_version())
        try:
            future.result(timeout=4.0)
        except Exception as e:
            log.warning(f"check_update network request failed/timeout: {e}")
            
        if self._github_version:
            curr = self.spp._config_cache.get("version", "1.0.10")
            has_new = self._github_version > curr
            return {
                "ok": True,
                "latest": self._github_version,
                "current": curr,
                "has_new": has_new,
                "message": f"Latest version: v{self._github_version}\nYour board version: v{curr}"
            }
        else:
            return {
                "ok": False,
                "message": "Failed to fetch updates from GitHub.\n获取 GitHub 在线更新版本失败，请检查网络！"
            }

    def start_smart_update(self) -> bool:
        """Automatically download latest firmware and trigger OTA flash."""
        if not self._connected or not self._github_version or not self._github_bin_url:
            return False
        # Run smart update in background thread to avoid blocking JS
        threading.Thread(target=lambda: run_coro(self._do_smart_update()), daemon=True).start()
        return True

    async def _do_smart_update(self):
        cache_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cache")
        os.makedirs(cache_dir, exist_ok=True)
        filename = f"esp32_bt_mic_v{self._github_version}.bin"
        save_path = os.path.join(cache_dir, filename)
        
        if os.path.exists(save_path) and os.path.getsize(save_path) > 100000:
            log.info("Found cached bin. Flashing...")
        else:
            log.info(f"Downloading from {self._github_bin_url}...")
            try:
                await self._download_firmware_async(self._github_bin_url, save_path)
            except Exception as e:
                log.error(f"Download failed: {e}")
                if self._window:
                    self._window.evaluate_js(f"alert('Download failed: {str(e)}')")
                return
                
        log.info("Flashing downloaded firmware...")
        def progress_cb(written, total):
            if self._window:
                self._window.evaluate_js(f"onOtaProgress({written}, {total}, 'flash')")

        try:
            ok = await self.spp.upload_firmware(save_path, progress_cb)
            if self._window:
                if ok:
                    self._window.evaluate_js("alert('OTA Upgrade Completed Successfully! The device is now rebooting.\\n固件升级成功！开发板正在重启，请稍候。')")
                else:
                    err_msg = getattr(self.spp, 'last_error', '') or '写入握手超时或串口未响应'
                    self._window.evaluate_js(f"alert('固件写入失败！\\n原因: {err_msg}\\n请检查供电线并复位重新连接测试！')")
        except Exception as e:
            log.error(f"Flash failed: {e}")
            if self._window:
                self._window.evaluate_js(f"alert('固件写入异常: {str(e)}')")

    async def _download_firmware_async(self, download_url, save_path):
        loop = asyncio.get_running_loop()
        def do_download():
            req = urllib.request.Request(download_url, headers={"User-Agent": "VoxTriple-App"})
            with urllib.request.urlopen(req) as response:
                total_size = int(response.headers.get('content-length', 0))
                bytes_read = 0
                with open(save_path, "wb") as f:
                    while True:
                        chunk = response.read(8192)
                        if not chunk:
                            break
                        f.write(chunk)
                        bytes_read += len(chunk)
                        if self._window and total_size > 0:
                            self._window.evaluate_js(f"onOtaProgress({bytes_read}, {total_size}, 'download')")
                            
        await loop.run_in_executor(None, do_download)

    def close_window(self):
        log.info("Close Window request received. Terminating safely...")
        def safe_exit():
            import time
            time.sleep(0.1)
            os._exit(0)
        threading.Thread(target=safe_exit, daemon=True).start()

    # ── Callbacks ─────────────────────────────────────────────────────────
    def _on_button_event(self, btn_id: int, state: int):
        log.info(f"Button trigger: {btn_id} state={state}")
        if self._window:
            self._window.evaluate_js(f"onPhysicalButtonEvent({btn_id}, {state})")

    def _on_status(self, hfp: int, audio: int):
        log.debug(f"HFP={hfp} Audio={audio}")

    async def _check_github_version(self):
        url = "https://api.github.com/repos/elementchen/VoxTriple_Classic/releases/latest"
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=4.0) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                tag = data.get("tag_name", "v1.0.17").strip()
                if tag.startswith("v"):
                    tag = tag[1:]
                self._github_version = tag
                for asset in data.get("assets", []):
                    name = asset.get("name", "")
                    if name.endswith(".bin") and "merged" not in name:
                        self._github_bin_url = asset.get("browser_download_url")
                        break
        except Exception as e:
            log.warning(f"Check version failed: {e}")

# ── Main Entry ────────────────────────────────────────────────────────────
def main():
    # Start asyncio background loop thread
    global _thread
    _thread = threading.Thread(target=start_asyncio_loop, daemon=True)
    _thread.start()

    # Determine index html assets directory (development vs compiled bundle)
    if hasattr(sys, "_MEIPASS"):
        index_path = os.path.join(sys._MEIPASS, "web", "index.html")
    else:
        index_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web", "index.html")

    api = Api()
    
    # Enable window stretching and provide proper content padding
    window = webview.create_window(
        title="VoxTriple Config Client", 
        url=index_path, 
        js_api=api,
        width=860,
        height=890,
        min_size=(830, 850),
        resizable=True,
        frameless=False
    )
    api.set_window(window)

    log.info("Starting PyWebView window...")
    webview.start(debug=False)

if __name__ == '__main__':
    main()
