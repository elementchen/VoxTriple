#!/usr/bin/env python3
from __future__ import annotations
"""VoxTriple — ESP32 BT Microphone Config (macOS - Webview Edition)

Shares spp_client.py with the Windows version.
Punctuation and keystrokes are captured natively in Webview via JS to avoid system crashes.
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
import subprocess
import webbrowser

# Import shared modules from sibling windows_app_python directory
_shared = os.path.join(os.path.dirname(__file__), "..", "windows_app_python")
if os.path.isdir(_shared):
    sys.path.insert(0, os.path.abspath(_shared))

import spp_client

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(name)s] %(message)s")
log = logging.getLogger("VoxTriple_Mac")

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
        """Scan Mac local for USB Serial ports, filter out virtual bluetooth ports."""
        ports = []
        for p in serial.tools.list_ports.comports():
            dev = p.device
            if "Bluetooth" not in dev and ("cu." in dev or "tty." in dev):
                ports.append(dev)
        return ports

    def connect_device(self, port: str) -> bool:
        """Connect to physical USB-serial device."""
        log.info(f"Connecting to {port}...")
        future = run_coro(self.spp.connect(port))
        try:
            ok = future.result(timeout=8.0)
        except Exception as e:
            log.error(f"connect_device exception: {e}")
            ok = False
        self._connected = ok
        return ok

    def disconnect_device(self) -> bool:
        """Disconnect current device."""
        log.info("Disconnecting serial port...")
        future = run_coro(self.spp.disconnect())
        future.result(timeout=3.0)
        self._connected = False
        return True

    def fetch_config(self, slot: int = None) -> dict | None:
        """Fetch config cache from the device."""
        if not self._connected:
            return None
            
        future = run_coro(self.spp.get_config(slot))
        cfg = future.result(timeout=3.0)
        if not cfg:
            return None
            
        query_os = cfg.get("query_os", 0)
        target_slot = cfg.get("query_slot", 0)

        # Format mapping list to JSON compatible
        config = {
            "version": cfg.get("version", "1.0.18"),
            "tx_power": cfg.get("tx_power", 7),
            "sleep_mode": cfg.get("sleep", 1),
            "mic_enabled": cfg.get("mic_enabled", 1),
            "board_model": cfg.get("board_model", 0),
            "board_name": cfg.get("board_name", "WEMOS 18650"),
            "sleep_timeout_min": cfg.get("sleep_timeout_min", 5),
            "active_slot": cfg.get("active_slot", 0),
            "query_slot": target_slot,
            "slot0_paired": cfg.get("slot0_paired", 0),
            "slot1_paired": cfg.get("slot1_paired", 0),
            "slot0_os": cfg.get("slot0_os", 0),
            "slot1_os": cfg.get("slot1_os", 0),
            "query_os": query_os,
            "mappings": cfg.get("buttons", [])
        }
        return config

    def write_config(self, mappings: list, tx: int, sleep: int, mic: int, sleep_timeout: int = 5, slot: int = None) -> bool:
        """Write all parameters to board Flash at once (Unified Sync)."""
        if not self._connected:
            return False
            
        log.info(f"Sync all parameters: mapping={mappings}, tx={tx}, sleep={sleep}, mic={mic}, sleep_timeout={sleep_timeout}, slot={slot}")
        future = run_coro(self.spp.set_config(mappings, tx, sleep, mic, sleep_timeout, slot))
        ok = future.result(timeout=6.0)
        return ok

    def switch_host_slot(self, slot: int = None) -> dict:
        """Switch active host slot (0: Host 1, 1: Host 2) via SPP."""
        if not self._connected:
            return {"status": "error", "message": "Not connected"}
        log.info(f"Requesting host switch to slot: {slot}")
        future = run_coro(self.spp.switch_host_slot(slot))
        try:
            ok = future.result(timeout=4.0)
            if ok:
                new_slot = self.spp._config_cache.get("active_slot", 0)
                return {"status": "ok", "active_slot": new_slot}
            return {"status": "error", "message": "Failed to switch host"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def set_board_model(self, model: int) -> dict:
        """Set board hardware model on device and trigger restart."""
        if not self._connected:
            return {"success": False, "message": "Not connected"}
        log.info(f"Set board hardware model: {model}")
        future = run_coro(self.spp.write_board_model(model))
        ok = future.result(timeout=5.0)
        return {"success": ok, "message": "ok" if ok else "write failed"}

    def set_sleep_timeout_min(self, minutes: int) -> dict:
        """Set inactivity sleep timeout (minutes) on device dynamically."""
        if not self._connected:
            return {"success": False, "message": "Not connected"}
        log.info(f"Set sleep timeout: {minutes} min")
        future = run_coro(self.spp.write_sleep_timeout_min(minutes))
        ok = future.result(timeout=5.0)
        return {"success": ok, "message": "ok" if ok else "write failed"}

    def reset_bt_pairing(self, slot: int = None) -> dict:
        """Expose API to frontend to forget BT pairings on the device and reboot."""
        if not self.spp or not self.spp.is_connected:
            return {"status": "error", "message": "Device not connected"}
        future = run_coro(self.spp.reset_bt_pairing(slot))
        try:
            ok = future.result(timeout=4.0)
            if ok:
                return {"status": "ok"}
            return {"status": "error", "message": "Command rejected"}
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def select_local_bin(self) -> str | None:
        """Select a local firmware file using macOS Finder dialog (via AppleScript to bypass Frameless Sandboxing)."""
        log.info("Opening Finder file selection dialog...")
        script = 'POSIX path of (choose file of type {"bin"} with prompt "Select VoxTriple firmware file (.bin):")'
        try:
            res = subprocess.run(
                ["osascript", "-e", script], 
                capture_output=True, 
                text=True, 
                timeout=30.0
            )
            if res.returncode == 0:
                path = res.stdout.strip()
                log.info(f"User selected file via AppleScript Finder: {path}")
                return path
        except Exception as e:
            log.warning(f"AppleScript file dialog failed: {e}, falling back to native webview dialog")
            
        # Fallback to standard webview dialog
        if self._window:
            file_types = ('Firmware Files (*.bin)', '*.bin')
            res = self._window.create_file_dialog(webview.OPEN_DIALOG, file_types=file_types)
            if res and len(res) > 0:
                return res[0]
        return None

    def trigger_ota(self, bin_path: str) -> bool:
        """Upload firmware to ESP32 via serial link."""
        if not self._connected:
            return False
            
        log.info(f"Starting OTA update with file: {bin_path}")
        
        def progress_cb(written, total):
            if self._window:
                try:
                    self._window.evaluate_js(f"onOtaProgress({written}, {total}, 'flash')")
                except Exception:
                    pass

        future = run_coro(self.spp.upload_firmware(bin_path, progress_cb))
        try:
            ok = future.result(timeout=180.0)
        except Exception as e:
            log.error(f"trigger_ota failed or timeout: {e}")
            ok = False

        if ok:
            # 升级成功，ESP32 正在重启，上位机断开连接以保持状态同步
            self._connected = False
            self.spp.disconnect_sync()
        return ok

    def check_update(self) -> dict:
        """Check GitHub latest update version and return results to JS."""
        future = run_coro(self._check_github_version())
        try:
            future.result(timeout=4.0)
        except Exception as e:
            log.warning(f"check_update network request failed/timeout: {e}")
            
        if self._github_version:
            curr = self.spp._config_cache.get("version", "1.0.18")
            has_new = self._is_newer_version(self._github_version, curr)
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

    def check_client_update(self, current_version: str) -> dict:
        """Check if a newer client application release exists on GitHub."""
        future = run_coro(self._check_github_version())
        try:
            future.result(timeout=4.0)
        except Exception as e:
            log.warning(f"check_client_update network request failed/timeout: {e}")
            
        if self._github_version:
            has_new = self._is_newer_version(self._github_version, current_version)
            return {
                "ok": True,
                "latest": self._github_version,
                "current": current_version,
                "has_new": has_new
            }
        return {
            "ok": False,
            "has_new": False
        }

    @staticmethod
    def _is_newer_version(latest_str: str, current_str: str) -> bool:
        """Compare two version strings semantically (e.g. 1.0.18 vs 1.0.17)."""
        import re
        def parse_v(v_str):
            if not v_str:
                return [0]
            clean = str(v_str).strip().lstrip("vV")
            parts = re.split(r"[.\-_]", clean)
            nums = []
            for p in parts:
                try:
                    nums.append(int(p))
                except ValueError:
                    if "beta" in p.lower():
                        nums.append(-1)
                    elif "rc" in p.lower():
                        nums.append(-2)
                    elif "alpha" in p.lower():
                        nums.append(-3)
            return nums

        l_nums = parse_v(latest_str)
        c_nums = parse_v(current_str)
        max_len = max(len(l_nums), len(c_nums))
        l_nums.extend([0] * (max_len - len(l_nums)))
        c_nums.extend([0] * (max_len - len(c_nums)))
        return l_nums > c_nums

    def open_browser_url(self, url: str = None) -> bool:
        """Open release or target URL in default system browser."""
        target = url or "https://github.com/elementchen/VoxTriple_Classic/releases"
        log.info(f"Opening browser URL: {target}")
        webbrowser.open(target)
        return True

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
                try:
                    self._window.evaluate_js(f"onOtaProgress({written}, {total}, 'flash')")
                except Exception:
                    pass

        try:
            ok = await self.spp.upload_firmware(save_path, progress_cb)
            if self._window:
                if ok:
                    self._window.evaluate_js("onSmartUpdateComplete(true)")
                else:
                    self._window.evaluate_js("onSmartUpdateComplete(false)")
        except Exception as e:
            log.error(f"OTA flash failed: {e}")
            if self._window:
                self._window.evaluate_js(f"onSmartUpdateError('{str(e)}')")

    async def _download_firmware_async(self, url: str, path: str):
        def _dl():
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=15.0) as resp:
                total = int(resp.headers.get("content-length", 0))
                downloaded = 0
                with open(path, "wb") as f:
                    while True:
                        chunk = resp.read(8192)
                        if not chunk:
                            break
                        f.write(chunk)
                        downloaded += len(chunk)
                        if self._window and total > 0:
                            try:
                                self._window.evaluate_js(f"onOtaProgress({downloaded}, {total}, 'download')")
                            except Exception:
                                pass
        await asyncio.to_thread(_dl)

    async def _check_github_version(self):
        url = "https://api.github.com/repos/elementchen/VoxTriple_Classic/releases/latest"
        def _req():
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=4.0) as resp:
                data = json.loads(resp.read().decode("utf-8"))
                tag = data.get("tag_name", "v1.0.18").strip()
                if tag.startswith("v"):
                    tag = tag[1:]
                self._github_version = tag
                for asset in data.get("assets", []):
                    name = asset.get("name", "")
                    if name.endswith(".bin") and "merged" not in name:
                        self._github_bin_url = asset.get("browser_download_url")
                        break
        try:
            await asyncio.to_thread(_req)
        except Exception as e:
            log.warning(f"Failed to query GitHub release: {e}")

    def close_window(self):
        if self._window:
            self._window.destroy()

    def _on_button_event(self, btn_id: int, state: int):
        if self._window:
            try:
                self._window.evaluate_js(f"onPhysicalButtonEvent({btn_id}, {state})")
            except Exception:
                pass

    def _on_status(self, msg: str):
        log.info(f"SPP status callback: {msg}")

# ── Main Entry ─────────────────────────────────────────────────────────────
def main():
    global _thread
    _thread = threading.Thread(target=start_asyncio_loop, daemon=True)
    _thread.start()

    # Determine index html assets directory (development vs compiled bundle)
    if hasattr(sys, "_MEIPASS"):
        index_path = os.path.join(sys._MEIPASS, "web", "index.html")
    else:
        index_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web", "index.html")

    api = Api()
    
    # 窗口尺寸固定为 980x905 并禁用缩放拉伸，视觉效果与 Windows 端完全统一
    window = webview.create_window(
        title="VoxTriple Config Client macOS", 
        url=index_path, 
        js_api=api,
        width=980,
        height=905,
        min_size=(980, 905),
        resizable=False,
        frameless=False
    )
    api.set_window(window)

    def setup_nswindow():
        try:
            from AppKit import NSWindowTitleHidden, NSWindowStyleMaskFullSizeContentView
            nswindow = window.native
            # 1. 隐藏标题栏文字
            nswindow.setTitleVisibility_(NSWindowTitleHidden)
            # 2. 标题栏透明且延伸网页视口到标题栏后方
            nswindow.setTitlebarAppearsTransparent_(True)
            nswindow.setStyleMask_(nswindow.styleMask() | NSWindowStyleMaskFullSizeContentView)
            # 3. 隐藏标题栏底部分割线 (macOS 11.0+)
            if hasattr(nswindow, 'setTitlebarSeparatorStyle_'):
                nswindow.setTitlebarSeparatorStyle_(0)  # NSWindowTitlebarSeparatorStyleNone = 0
            log.info("Cocoa NSWindow styling successfully applied.")
        except Exception as e:
            log.warning(f"Failed to style native NSWindow: {e}")

    log.info("Starting PyWebView window...")
    webview.start(setup_nswindow, debug=False)

if __name__ == '__main__':
    main()
