"""Classic Bluetooth SPP Client for VoxTriple.

Replaces the BleClient (ble_client.py) using pyserial.
Bridges serial sync-read thread to asyncio futures for smooth GUI interaction.
"""
import os
import sys
import re
import time
import asyncio
import json
import logging
import threading
import serial
import serial.tools.list_ports

log = logging.getLogger(__name__)


class _EsptoolProgressWriter:
    """Captures esptool stdout/stderr to extract flash write percentage and invoke GUI progress callback."""
    def __init__(self, orig_stdout, total_size, cb=None):
        self.orig_stdout = orig_stdout
        self.total_size = total_size
        self.cb = cb
        # Matches `]  35.2%` (esptool v4+), `( 35 %)` (esptool v3-), or bare `35.2%`
        self.pct_re = re.compile(r'(?:\]|\(|\b)\s*(\d+(?:\.\d+)?)\s*%')
        self.encoding = getattr(orig_stdout, "encoding", "utf-8") or "utf-8"
        self.errors = getattr(orig_stdout, "errors", "replace") or "replace"
        self._last_pct = -1.0
        self._buffer = ""

    def isatty(self):
        return False

    def write(self, text):
        if self.orig_stdout:
            try:
                self.orig_stdout.write(text)
            except Exception:
                pass

        if not text:
            return

        self._buffer += text
        if len(self._buffer) > 2048:
            self._buffer = self._buffer[-1024:]

        matches = self.pct_re.findall(self._buffer)
        if matches:
            try:
                pct = float(matches[-1])
                if pct > self._last_pct + 0.3 or pct >= 100.0:
                    self._last_pct = pct
                    if self.cb:
                        written = int(self.total_size * min(pct, 100.0) / 100.0)
                        self.cb(written, self.total_size)
            except Exception:
                pass

    def flush(self):
        if self.orig_stdout:
            try:
                self.orig_stdout.flush()
            except Exception:
                pass



class SppClient:
    """Connects to ESP32 SPP server via virtual COM port."""

    def __init__(self):
        self._ser = None
        self._port = None
        self._connected = False
        self._read_thread = None
        self._loop = None
        
        # Callbacks registered by the GUI
        self.on_button_event = None   # callback(button_id: int, state: int)
        self.on_status = None         # callback(hfp: int, audio: int)

        # Future used to wait for responses of commands
        self._response_future = None
        
        # Local configuration cache populated on connection
        self._config_cache = {}
        
        # OTA mode flag and queue for flow control
        self._ota_mode = False
        self._ota_queue = asyncio.Queue()

    @property
    def is_connected(self) -> bool:
        return self._connected

    @property
    def address(self) -> str | None:
        """Expose the current COM port name to mimic the original BLE address API."""
        return self._port

    @staticmethod
    def list_ports() -> list[dict]:
        """List all COM ports, sorting likely Bluetooth ports to the top."""
        ports = serial.tools.list_ports.comports()
        result = []
        for p in ports:
            name = p.device
            desc = p.description or ""
            hwid = p.hwid or ""
            
            # Identify potential bluetooth virtual serial ports
            is_bt = False
            bt_keywords = ["bluetooth", "btport", "bthenum", "蓝牙", "标准串行"]
            if any(kw in desc.lower() or kw in hwid.lower() for kw in bt_keywords):
                is_bt = True
                
            result.append({
                "port": name,
                "desc": f"{name} ({desc})",
                "is_bt": is_bt
            })
            
        # Sort so that Bluetooth ports appear first
        result.sort(key=lambda x: x["is_bt"], reverse=True)
        return result

    async def connect(self, port: str) -> bool:
        """Open the serial port, start the read thread, and fetch configuration."""
        try:
            log.info(f"Connecting to serial port {port}...")
            # Configure PySerial without triggering auto-reset flags where supported
            self._ser = serial.Serial()
            self._ser.port = port
            self._ser.baudrate = 115200
            self._ser.timeout = 1.0
            self._ser.rtscts = False
            self._ser.dsrdtr = False
            self._ser.open()
            
            # Explicitly clear DTR and RTS pin levels to try avoiding ESP32 reset
            try:
                self._ser.dtr = False
                self._ser.rts = False
            except Exception:
                pass
                
            self._port = port
            self._connected = True
            
            # Capture the current asyncio event loop to dispatch events safely
            self._loop = asyncio.get_running_loop()
            
            # Start background reading thread
            self._read_thread = threading.Thread(target=self._run_read_loop, daemon=True)
            self._read_thread.start()
            
            # Fast-path check: give driver 0.3s to establish UART stream
            log.info("Checking device readiness...")
            await asyncio.sleep(0.3)
            
            # Fetch device configuration
            ok = await self._fetch_config()
            if not ok:
                log.info("Device did not respond immediately, waiting 1.0s for possible boot...")
                await asyncio.sleep(1.0)
                ok = await self._fetch_config()
                
            if ok:
                log.info(f"SPP Client successfully connected to {port}")
            else:
                log.warning(f"Device at {port} connected (config fetch timed out, keeping connection for OTA/console).")
                
            return True
        except Exception as e:
            log.error(f"Failed to connect to {port}: {e}")
            self.disconnect_sync()
            return False

    async def disconnect(self):
        """Asynchronous disconnect wrapper."""
        self.disconnect_sync()

    def disconnect_sync(self):
        """Synchronously close the port and join reader thread."""
        self._connected = False
        if self._ser:
            try:
                self._ser.close()
            except Exception:
                pass
            self._ser = None
        self._port = None
        self._config_cache.clear()
        log.info("SPP Client disconnected")

    def _run_read_loop(self):
        """Thread worker to continuously read from the serial port line by line."""
        while self._connected and self._ser:
            try:
                line_bytes = self._ser.readline()
                if not line_bytes:
                    continue
                    
                line = line_bytes.decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                    
                log.debug(f"Serial RX: {line}")
                if not line.startswith("{"):
                    log.warning(f"[ESP32 LOG] {line}")
                    continue
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    # Robust fallback parser for noisy serial lines (crosstalk / power ripple bit-flips)
                    import re
                    recovered = False
                    data = {}
                    if "next" in line:
                        tw_match = re.search(r'"total_written"\s*:\s*(\d+)', line)
                        if tw_match:
                            data = {"status": "next", "total_written": int(tw_match.group(1))}
                            recovered = True
                    elif "done" in line:
                        data = {"status": "done"}
                        recovered = True
                    elif "error" in line:
                        data = {"status": "error"}
                        reason_match = re.search(r'"reason"\s*:\s*"([^"]+)"', line)
                        data["reason"] = reason_match.group(1) if reason_match else "unknown_recovered"
                        recovered = True
                        
                    if recovered:
                        log.warning(f"Recovered JSON structure from noisy line: {line} -> {data}")
                    else:
                        log.warning(f"[ESP32 LOG (JSON Parse Fail)] {line}")
                        continue
                
                # Check if it's an unsolicited event from ESP32
                if "event" in data:
                    evt = data["event"]
                    if evt == "btn" and self.on_button_event:
                        btn_id = data.get("id", 0)
                        state = data.get("state", 0)
                        self.on_button_event(btn_id, state)
                    elif evt == "status" and self.on_status:
                        hfp = data.get("hfp", 0)
                        audio = data.get("audio", 0)
                        self.on_status(hfp, audio)
                        
                # Check if it's a command response
                elif "status" in data:
                    if self._ota_mode:
                        self._loop.call_soon_threadsafe(self._ota_queue.put_nowait, data)
                    else:
                        # Dispatch to the waiting future with race condition protection
                        fut = self._response_future
                        if fut and not fut.done():
                            self._response_future = None  # Block subsequent arrivals from touching this future
                            self._loop.call_soon_threadsafe(fut.set_result, data)
                        
            except Exception as e:
                log.error(f"Error in read loop: {e}")
                if self._connected:
                    # Trigger disconnect in case of hardware error
                    self._loop.call_soon_threadsafe(self.disconnect_sync)
                break

    async def _send_cmd(self, cmd_dict: dict, timeout: float = 2.0) -> dict | None:
        """Send a JSON command and wait asynchronously for its response."""
        if not self._connected or not self._ser:
            return None
            
        # Create a new future for this command
        self._response_future = self._loop.create_future()
        
        try:
            cmd_str = json.dumps(cmd_dict) + "\n"
            self._ser.write(cmd_str.encode("utf-8"))
            
            # Wait for response future to resolve
            resp = await asyncio.wait_for(self._response_future, timeout=timeout)
            return resp
        except Exception as e:
            log.error(f"Command {cmd_dict.get('cmd')} failed: {e}")
            return None
        finally:
            self._response_future = None

    async def _fetch_config(self) -> bool:
        """Fetch the full configuration from ESP32 and save in cache."""
        for attempt in range(3):
            log.info(f"Fetching configuration from device (attempt {attempt + 1}/3)...")
            resp = await self._send_cmd({"cmd": "get_config"}, timeout=1.5)
            if resp and resp.get("status") == "ok":
                self._config_cache = resp
                return True
            await asyncio.sleep(1.0)
        return False

    async def read_button_mapping(self, idx: int) -> tuple[int, int] | None:
        """Return the (vk_code, modifier) for button index from cache."""
        # Use cached values. If not cached, attempt a fetch
        if not self._config_cache:
            await self._fetch_config()
            
        vk_key = f"btn{idx+1}_vk"
        mod_key = f"btn{idx+1}_mod"
        
        if vk_key in self._config_cache and mod_key in self._config_cache:
            return self._config_cache[vk_key], self._config_cache[mod_key]
        return None

    async def write_button_mapping(self, idx: int, vk: int, mod: int) -> bool:
        """Send command to save button mapping on the ESP32."""
        resp = await self._send_cmd({
            "cmd": "set_btn",
            "idx": idx,
            "vk": vk,
            "mod": mod
        })
        if resp and resp.get("status") == "ok":
            # Update local cache
            self._config_cache[f"btn{idx+1}_vk"] = vk
            self._config_cache[f"btn{idx+1}_mod"] = mod
            return True
        return False

    async def read_tx_power(self) -> int | None:
        """Get the Classic BT TX power level from cache."""
        if not self._config_cache:
            await self._fetch_config()
        return self._config_cache.get("tx_power")

    async def write_tx_power(self, level: int) -> bool:
        """Save Classic BT TX power level to the ESP32."""
        resp = await self._send_cmd({
            "cmd": "set_tx_power",
            "level": level
        })
        if resp and resp.get("status") == "ok":
            self._config_cache["tx_power"] = level
            return True
        return False

    async def read_sleep_mode(self) -> int | None:
        """Get the sleep mode enabled flag from cache."""
        if not self._config_cache:
            await self._fetch_config()
        return self._config_cache.get("sleep_mode")

    async def write_sleep_mode(self, enabled: int) -> bool:
        """Save sleep mode configuration to the ESP32."""
        resp = await self._send_cmd({
            "cmd": "set_sleep_mode",
            "enabled": enabled
        })
        if resp and resp.get("status") == "ok":
            self._config_cache["sleep_mode"] = enabled
            return True
        return False

    async def read_mic_enabled(self) -> int | None:
        """Get the mic enabled flag from cache."""
        if not self._config_cache:
            await self._fetch_config()
        return self._config_cache.get("mic_enabled")

    async def write_mic_enabled(self, enabled: int) -> bool:
        """Save mic enabled configuration to the ESP32."""
        resp = await self._send_cmd({
            "cmd": "set_mic_enabled",
            "enabled": enabled
        })
        if resp and resp.get("status") == "ok":
            self._config_cache["mic_enabled"] = enabled
            return True
        return False

    async def read_board_model(self) -> int | None:
        """Get the hardware board model from cache (0: WEMOS 18650, 1: ESP32 Lite V1.0.0)."""
        if not self._config_cache:
            await self._fetch_config()
        return self._config_cache.get("board_model", 0)

    async def write_board_model(self, model: int) -> bool:
        """Save hardware board model to the ESP32 and trigger reboot."""
        resp = await self._send_cmd({
            "cmd": "set_board_model",
            "model": model
        })
        if resp and resp.get("status") == "ok":
            self._config_cache["board_model"] = model
            return True
        return False

    async def read_sleep_timeout_min(self) -> int | None:
        """Get the inactivity deep sleep timeout (in minutes) from cache."""
        if not self._config_cache:
            await self._fetch_config()
        return self._config_cache.get("sleep_timeout_min", 5)

    async def write_sleep_timeout_min(self, minutes: int) -> bool:
        """Save inactivity sleep timeout (minutes) to the ESP32 and trigger reboot."""
        resp = await self._send_cmd({
            "cmd": "set_sleep_timeout",
            "minutes": minutes
        })
        if resp and resp.get("status") == "ok":
            self._config_cache["sleep_timeout_min"] = minutes
            return True
        return False

    async def reset_bt_pairing(self, slot: int = None) -> bool:
        """Send command to reset Bluetooth pairings on ESP32 for target slot (forget device & reboot)."""
        cmd = {"cmd": "reset_bt_pairing"}
        if slot is not None:
            cmd["slot"] = slot
        resp = await self._send_cmd(cmd)
        return resp is not None and resp.get("status") == "ok"

    async def switch_host_slot(self, slot: int = None) -> bool:
        """Switch active Bluetooth host slot (0: PC 1, 1: PC 2)."""
        cmd_dict = {"cmd": "switch_host_slot"}
        if slot is not None:
            cmd_dict["slot"] = slot
        resp = await self._send_cmd(cmd_dict)
        if resp and resp.get("status") == "ok":
            if slot is not None:
                self._config_cache["active_slot"] = slot
            else:
                self._config_cache["active_slot"] = 1 if self._config_cache.get("active_slot", 0) == 0 else 0
            return True
        return False

    async def get_config(self, slot: int = None) -> dict | None:
        """Fetch and return full configuration dictionary formatted for UI client."""
        if slot is not None:
            resp = await self._send_cmd({"cmd": "get_config", "slot": slot}, timeout=2.0)
            if resp and resp.get("status") == "ok":
                self._config_cache.update(resp)
        else:
            ok = await self._fetch_config()
            if not ok:
                return None
        
        cfg = {
            "buttons": [
                {"vk": self._config_cache.get("btn1_vk", 0xA3), "mod": self._config_cache.get("btn1_mod", 0)},
                {"vk": self._config_cache.get("btn2_vk", 0x0D), "mod": self._config_cache.get("btn2_mod", 0)},
                {"vk": self._config_cache.get("btn3_vk", 0x26), "mod": self._config_cache.get("btn3_mod", 0)},
                {"vk": self._config_cache.get("btn4_vk", 0x28), "mod": self._config_cache.get("btn4_mod", 0)}
            ],
            "tx_power": self._config_cache.get("tx_power", 4),
            "sleep": self._config_cache.get("sleep_mode", 1),
            "mic_enabled": self._config_cache.get("mic_enabled", 1),
            "version": self._config_cache.get("version", "1.0.18"),
            "board_model": self._config_cache.get("board_model", 0),
            "board_name": self._config_cache.get("board_name", "WEMOS 18650"),
            "sleep_timeout_min": self._config_cache.get("sleep_timeout_min", 5),
            "active_slot": self._config_cache.get("active_slot", 0),
            "query_slot": self._config_cache.get("query_slot", slot if slot is not None else self._config_cache.get("active_slot", 0)),
            "slot0_paired": self._config_cache.get("slot0_paired", 0),
            "slot1_paired": self._config_cache.get("slot1_paired", 0),
            "slot0_os": self._config_cache.get("slot0_os", 0),
            "slot1_os": self._config_cache.get("slot1_os", 0),
            "query_os": self._config_cache.get("query_os", 0)
        }
        return cfg

    async def write_host_os(self, slot: int, os_type: int) -> bool:
        """Set the identified OS type (1: Apple, 2: Windows) on the device for the given slot."""
        resp = await self._send_cmd({"cmd": "set_host_os", "slot": slot, "os_type": os_type})
        if resp and resp.get("status") == "ok":
            self._config_cache[f"slot{slot}_os"] = os_type
            if self._config_cache.get("active_slot", 0) == slot:
                self._config_cache["query_os"] = os_type
            return True
        return False

    async def set_config(self, mapping: list, tx: int, sleep: int, mic_enabled: int = 1, sleep_timeout_min: int = 5, slot: int = None, os_type: int = None) -> bool:
        """Write all configurations at once into ESP32 via set_all_config single atomic command."""
        cmd_dict = {
            "cmd": "set_all_config",
            "tx_power": tx,
            "sleep_mode": sleep,
            "mic_enabled": mic_enabled,
            "sleep_timeout_min": sleep_timeout_min
        }
        if slot is not None:
            cmd_dict["slot"] = slot
        if os_type is not None:
            cmd_dict["os_type"] = os_type
        for i in range(min(len(mapping), 4)):
            cmd_dict[f"btn{i+1}_vk"] = mapping[i]["vk"]
            cmd_dict[f"btn{i+1}_mod"] = mapping[i]["mod"]
            
        resp = await self._send_cmd(cmd_dict, timeout=3.0)
        if resp and resp.get("status") == "ok":
            return True
        return False

    async def read_firmware_version(self) -> str:
        """Get the current firmware version from cache."""
        if not self._config_cache:
            await self._fetch_config()
        return self._config_cache.get("version", "Unknown")

    async def upload_firmware(self, bin_path: str, progress_callback=None, mode="ota") -> bool:
        """Upload firmware .bin file to ESP32 using reliable application-level serial OTA."""
        if not self._connected or not self._ser:
            log.error("Flash failed: Serial client is not connected.")
            return False

        if not os.path.isfile(bin_path):
            log.error(f"Firmware binary file not found: {bin_path}")
            return False

        total_size = os.path.getsize(bin_path)
        port = self._port or (self._ser.port if self._ser else None)
        log.info(f"Starting firmware OTA upload on {port}. File: {bin_path} ({total_size} bytes, Mode: {mode})")

        # 始终使用纯应用层串口 OTA 协议，完全不触碰 DTR/RTS 硬件电平，杜绝芯片挂死
        if mode == "esptool":
            log.info("Executing Direct Hardware Flash Engine (esptool)...")
            return await self._upload_firmware_esptool(bin_path, total_size, port, progress_callback)
        else:
            log.info("Executing Reliable Application Serial OTA Engine...")
            return await self._upload_firmware_ota(bin_path, total_size, progress_callback)

    async def _upload_firmware_esptool(self, bin_path: str, total_size: int, port: str, progress_callback=None) -> bool:
        """Directly flash ESP32 via esptool Python API, cross-platform compatible with CH340 & CP2102."""
        import esptool

        # Smart flash offset:
        # Full merged image (> 1MB or containing 'merged') flashes at 0x0
        # Single App partition (standard OTA bin ~930KB) flashes at 0x20000
        is_merged = (total_size > 1000000) or ("merged" in os.path.basename(bin_path).lower())
        flash_offset = "0x0" if is_merged else "0x20000"
        log.info(f"Direct Flash: Target flash offset: {flash_offset} (Merged: {is_merged})")

        # 1. Release serial port so esptool has exclusive physical access
        log.info(f"Direct Flash: Closing pyserial handle on {port}...")
        self.disconnect_sync()
        await asyncio.sleep(1.0)

        # 2. Run esptool in thread pool executor to prevent blocking asyncio loop
        loop = asyncio.get_running_loop()

        def do_esptool(baud: int) -> bool:
            old_stdout = sys.stdout
            old_stderr = sys.stderr
            writer = _EsptoolProgressWriter(old_stdout, total_size, progress_callback)
            sys.stdout = writer
            sys.stderr = writer
            args = [
                "--chip", "esp32",
                "--port", port,
                "--baud", str(baud),
                "--before", "default-reset",
                "--after", "hard-reset",
                "write-flash",
                "--flash-mode", "dio",
                "--flash-size", "4MB",
                "--flash-freq", "40m",
                flash_offset, bin_path
            ]
            try:
                log.info(f"Invoking esptool.main on {port} with baud {baud}...")
                esptool.main(args)
                return True
            except SystemExit as se:
                if se.code != 0:
                    self.last_error = f"esptool exit code {se.code}"
                    log.error(f"esptool exited with non-zero exit code: {se.code}")
                return (se.code == 0)
            except Exception as e:
                import traceback
                self.last_error = str(e)
                log.error(f"esptool execution error at {baud} baud: {e}\n{traceback.format_exc()}")
                return False
            finally:
                sys.stdout = old_stdout
                sys.stderr = old_stderr

        # Try high speed 460800 first (takes only ~13s)
        flash_ok = await loop.run_in_executor(None, do_esptool, 460800)
        if not flash_ok:
            log.warning("High-speed 460800 flash attempt failed. Retrying at 115200 baud...")
            await asyncio.sleep(1.0)
            flash_ok = await loop.run_in_executor(None, do_esptool, 115200)

        if not flash_ok:
            log.error(f"Direct esptool flash failed: {getattr(self, 'last_error', 'unknown')}")
            # Attempt to reconnect anyway so GUI remains operational
            await self.connect(port)
            return False

        log.info("Direct flash succeeded! Device is rebooting via hardware reset pulse.")
        if progress_callback:
            try:
                progress_callback(total_size, total_size)
            except Exception:
                pass

        # 3. Disconnect port cleanly so device can reboot independently and UI resets connection
        self.disconnect_sync()
        log.info("SPP serial handle closed after flash. Awaiting user reconnection.")
        return True

    async def _upload_firmware_ota(self, bin_path: str, total_size: int, progress_callback=None) -> bool:
        """Application-level serial OTA protocol engine with Clean OTA Mode handshake & strict 1KB sync."""
        try:
            with open(bin_path, "rb") as f:
                bin_data = f.read()
        except Exception as e:
            log.error(f"Failed to read firmware binary file: {e}")
            return False

        # Clear any stale data in ota_queue
        while not self._ota_queue.empty():
            self._ota_queue.get_nowait()

        # 1. Send ota_start command to initiate OTA handshake
        log.info(f"Sending ota_start command to ESP32 (Size: {total_size} bytes)...")
        resp = await self._send_cmd({"cmd": "ota_start", "size": total_size}, timeout=8.0)
        if not resp:
            log.error("OTA start failed: No response from device.")
            return False

        # 检查是否需要进入 Clean OTA Mode（关闭蓝牙/音频以彻底消除 Flash 写入冲突）
        if resp.get("status") == "ok" and resp.get("reboot") == 1:
            log.info("Device requested reboot into clean OTA mode for zero-interference flash erase...")
            port = self._ser.port if self._ser else self._port
            self.disconnect_sync()

            # 等待 3.5 秒让设备软重启并就绪
            await asyncio.sleep(3.5)

            log.info(f"Reconnecting to {port} in clean OTA mode...")
            if not await self.connect(port):
                log.error("Failed to reconnect in clean OTA mode.")
                return False

            log.info("Re-sending OTA start handshake in clean mode...")
            resp = await self._send_cmd({"cmd": "ota_start", "size": total_size}, timeout=8.0)
            if not resp or resp.get("status") != "ok":
                log.error(f"Clean OTA start failed: {resp.get('reason') if resp else 'No response'}")
                return False

        elif resp.get("status") != "ok":
            log.error(f"OTA start failed: {resp.get('reason', 'unknown error')}")
            return False

        log.info("OTA handshake success! Streaming binary payload in strict 1KB chunks...")
        self._ota_mode = True
        try:
            chunk_size = 1024
            current_total = 0
            last_cb_time = 0.0

            # Initial 0% progress callback
            if progress_callback:
                try:
                    progress_callback(0, total_size)
                except Exception:
                    pass

            for i in range(0, total_size, chunk_size):
                chunk = bin_data[i:i+chunk_size]
                expected_total = i + len(chunk)

                self._ser.write(chunk)
                self._ser.flush()

                # Strict 1KB handshake flow control:
                # 严格等待 ESP32 写入 Flash 并确认，彻底杜绝 UART RingBuffer 溢出和数据损坏！
                while current_total < expected_total:
                    try:
                        chunk_resp = await asyncio.wait_for(self._ota_queue.get(), timeout=8.0)
                        if not chunk_resp:
                            log.error("Empty chunk response from device.")
                            return False

                        if chunk_resp.get("status") == "error":
                            log.error(f"OTA chunk write failed: {chunk_resp.get('reason')}")
                            return False
                        if chunk_resp.get("status") == "done":
                            log.info("OTA completed successfully!")
                            if progress_callback:
                                try:
                                    progress_callback(total_size, total_size)
                                except Exception:
                                    pass
                            self.disconnect_sync()
                            return True

                        current_total = chunk_resp.get("total_written", 0)
                    except asyncio.TimeoutError:
                        log.error(f"OTA chunk write timed out waiting for {expected_total} bytes. Current confirmed: {current_total}")
                        return False

                # 节流更新进度条（避免高频同步阻塞 evaluate_js）
                now = time.time()
                if (now - last_cb_time >= 0.08) or (expected_total >= total_size):
                    last_cb_time = now
                    if progress_callback:
                        try:
                            progress_callback(current_total, total_size)
                        except Exception:
                            pass

                # 每次 chunk 给驱动 3ms 稳定延迟
                await asyncio.sleep(0.003)

            # 3. 所有分包发送完毕，等待 ESP32 执行 esp_ota_end 校验与设置引导分区
            log.info("All firmware chunks sent. Waiting for final verification on device...")
            try:
                final_resp = await asyncio.wait_for(self._ota_queue.get(), timeout=12.0)
                if final_resp and final_resp.get("status") == "done":
                    log.info("OTA upgrade completed successfully! The device is now rebooting.")
                    if progress_callback:
                        try:
                            progress_callback(total_size, total_size)
                        except Exception:
                            pass
                    self.disconnect_sync()
                    return True
                else:
                    log.error(f"OTA verification failed: {final_resp.get('reason') if final_resp else 'Invalid status'}")
                    return False
            except asyncio.TimeoutError:
                log.error("OTA final verification timed out.")
                return False
        finally:
            self._ota_mode = False
