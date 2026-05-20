"""JSON config persistence — mirrors the C# ConfigurationService."""
import json
import os
import logging
import sys

log = logging.getLogger(__name__)

# config saved in %APPDATA%/VoxTriple/config.json (same location as C# version)
if sys.platform == "win32":
    _config_dir = os.path.join(os.environ.get("APPDATA", os.path.expanduser("~")), "VoxTriple")
else:
    _config_dir = os.path.join(os.path.expanduser("~"), ".voxtriple")

_config_file = os.path.join(_config_dir, "config.json")


def load() -> dict:
    try:
        if os.path.exists(_config_file):
            with open(_config_file, "r", encoding="utf-8") as f:
                return json.load(f)
    except Exception as e:
        log.warning(f"Failed to load config: {e}")
    return {
        "device_address": "",
        "ble_address": 0,
        "auto_start": False,
        "tx_power": 7,
        "sleep_mode": False,
        "button1": {"vk_code": 0x0D, "modifier": 0x00},
        "button2": {"vk_code": 0x1B, "modifier": 0x00},
        "button3": {"vk_code": 0x20, "modifier": 0x00},
    }


def save(cfg: dict):
    try:
        os.makedirs(_config_dir, exist_ok=True)
        with open(_config_file, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2, ensure_ascii=False)
    except Exception as e:
        log.error(f"Failed to save config: {e}")
