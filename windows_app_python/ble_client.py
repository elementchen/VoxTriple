"""BLE GATT client for ESP32_BT_MIC — button mapping service (0x1820)."""
import asyncio
import logging
from bleak import BleakScanner, BleakClient

log = logging.getLogger(__name__)

SERVICE_UUID     = "00001820-0000-1000-8000-00805f9b34fb"
CHAR_BTN1_MAP    = "00002a01-0000-1000-8000-00805f9b34fb"
CHAR_BTN2_MAP    = "00002a02-0000-1000-8000-00805f9b34fb"
CHAR_BTN3_MAP    = "00002a03-0000-1000-8000-00805f9b34fb"
CHAR_BTN_EVENT   = "00002a04-0000-1000-8000-00805f9b34fb"
CHAR_DEV_STATUS  = "00002a05-0000-1000-8000-00805f9b34fb"


class BleClient:
    """Connects to ESP32_BT_MIC over BLE, reads/writes button mappings,
    and receives button press/release notifications."""

    def __init__(self):
        self._client: BleakClient | None = None
        self._address: str | None = None
        self._connected = False
        self.on_button_event = None   # callback(button_id: int, state: int)
        self.on_status = None         # callback(hfp: int, audio: int)

    @property
    def is_connected(self) -> bool:
        return self._connected

    @property
    def address(self) -> str | None:
        return self._address

    @staticmethod
    async def scan(timeout: float = 10.0) -> str | None:
        """Scan for ESP32_BT_MIC, return BLE address or None."""
        log.info("Scanning for ESP32_BT_MIC…")
        device = await BleakScanner.find_device_by_filter(
            lambda d, ad: ad.local_name and "ESP32" in ad.local_name,
            timeout=timeout,
        )
        if device:
            log.info(f"Found: {device.name} [{device.address}]")
            return device.address
        log.warning("ESP32_BT_MIC not found")
        return None

    async def connect(self, address: str) -> bool:
        """Connect by BLE address, discover GATT, subscribe to notifications."""
        try:
            self._client = BleakClient(address)
            await self._client.connect()
            log.info("BLE connected, discovering GATT…")
            await asyncio.sleep(1.5)  # wait for ESP32 to register all chars

            # Subscribe to button events
            await self._client.start_notify(CHAR_BTN_EVENT, self._on_button_event)
            # Subscribe to device status
            await self._client.start_notify(CHAR_DEV_STATUS, self._on_status)

            self._connected = True
            self._address = address
            log.info("BLE ready — notifications subscribed")
            return True
        except Exception as e:
            log.error(f"BLE connect failed: {e}")
            self._connected = False
            return False

    async def disconnect(self):
        if self._client and self._client.is_connected:
            await self._client.disconnect()
        self._connected = False

    async def read_button_mapping(self, idx: int) -> tuple[int, int] | None:
        """Read [vk_code, modifier] for button 0/1/2."""
        uuids = [CHAR_BTN1_MAP, CHAR_BTN2_MAP, CHAR_BTN3_MAP]
        if idx < 0 or idx > 2 or not self._client:
            return None
        try:
            data = await self._client.read_gatt_char(uuids[idx])
            if len(data) >= 2:
                return data[0], data[1]
        except Exception as e:
            log.error(f"Read btn{idx+1} failed: {e}")
        return None

    async def write_button_mapping(self, idx: int, vk: int, mod: int) -> bool:
        """Write [vk_code, modifier] for button 0/1/2."""
        uuids = [CHAR_BTN1_MAP, CHAR_BTN2_MAP, CHAR_BTN3_MAP]
        if idx < 0 or idx > 2 or not self._client:
            return False
        try:
            await self._client.write_gatt_char(uuids[idx], bytes([vk, mod]), response=True)
            return True
        except Exception as e:
            log.error(f"Write btn{idx+1} failed: {e}")
            return False

    def _on_button_event(self, _sender, data: bytearray):
        if len(data) >= 2 and self.on_button_event:
            self.on_button_event(data[0], data[1])

    def _on_status(self, _sender, data: bytearray):
        if len(data) >= 2 and self.on_status:
            self.on_status(data[0], data[1])
