"""BLE GATT client for ESP32_BT_MIC — button mapping service (0x1820).

Characteristic UUIDs (0x2A01-0x2A05) conflict with Bluetooth SIG-defined
standard characteristics.  We MUST discover the service first, then resolve
each characteristic by handle within the service scope — bleak's UUID-only
API throws "Multiple Characteristics with this UUID" otherwise.
"""
import asyncio
import logging
from bleak import BleakScanner, BleakClient
from bleak.backends.characteristic import BleakGATTCharacteristic

log = logging.getLogger(__name__)

SERVICE_UUID     = "00001820-0000-1000-8000-00805f9b34fb"
CHAR_BTN1_MAP    = "00002a01-0000-1000-8000-00805f9b34fb"
CHAR_BTN2_MAP    = "00002a02-0000-1000-8000-00805f9b34fb"
CHAR_BTN3_MAP    = "00002a03-0000-1000-8000-00805f9b34fb"
CHAR_BTN_EVENT   = "00002a04-0000-1000-8000-00805f9b34fb"
CHAR_DEV_STATUS  = "00002a05-0000-1000-8000-00805f9b34fb"
CHAR_TX_POWER    = "00002a06-0000-1000-8000-00805f9b34fb"
CHAR_SLEEP_MODE  = "00002a07-0000-1000-8000-00805f9b34fb"


class BleClient:
    """Connects to ESP32_BT_MIC over BLE, reads/writes button mappings,
    and receives button press/release notifications."""

    def __init__(self):
        self._client: BleakClient | None = None
        self._address: str | None = None
        self._connected = False
        self.on_button_event = None   # callback(button_id: int, state: int)
        self.on_status = None         # callback(hfp: int, audio: int)

        # Characteristic handles (resolved from our service only)
        self._ch = [None, None, None]       # BTN1_MAP, BTN2_MAP, BTN3_MAP
        self._ch_event: BleakGATTCharacteristic | None = None
        self._ch_status: BleakGATTCharacteristic | None = None
        self._ch_tx_power: BleakGATTCharacteristic | None = None
        self._ch_sleep_mode: BleakGATTCharacteristic | None = None

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
        """Connect, discover our service, resolve char handles, subscribe."""
        try:
            self._client = BleakClient(address)
            await self._client.connect()
            log.info("BLE connected, waiting for service registration…")
            await asyncio.sleep(1.5)

            # Discover services and find our custom service 0x1820
            svc = None
            for s in self._client.services:
                if s.uuid.lower() == SERVICE_UUID:
                    svc = s
                    break
            if svc is None:
                log.error(f"Service {SERVICE_UUID[:8]} not found")
                return False

            # Resolve characteristics by handle WITHIN our service
            char_uuids = [CHAR_BTN1_MAP, CHAR_BTN2_MAP, CHAR_BTN3_MAP]
            for i, uid in enumerate(char_uuids):
                for c in svc.characteristics:
                    if c.uuid.lower() == uid:
                        self._ch[i] = c
                        break

            for c in svc.characteristics:
                if c.uuid.lower() == CHAR_BTN_EVENT:
                    self._ch_event = c
                elif c.uuid.lower() == CHAR_DEV_STATUS:
                    self._ch_status = c
                elif c.uuid.lower() == CHAR_TX_POWER:
                    self._ch_tx_power = c
                elif c.uuid.lower() == CHAR_SLEEP_MODE:
                    self._ch_sleep_mode = c

            log.info(f"Resolved: btn1={self._ch[0] is not None} btn2={self._ch[1] is not None} "
                     f"btn3={self._ch[2] is not None} evt={self._ch_event is not None} "
                     f"st={self._ch_status is not None} tx={self._ch_tx_power is not None} "
                     f"sl={self._ch_sleep_mode is not None}")

            # Subscribe — use characteristic objects (handle-based, no UUID ambiguity)
            if self._ch_event:
                await self._client.start_notify(self._ch_event, self._on_button_event)
            if self._ch_status:
                await self._client.start_notify(self._ch_status, self._on_status)

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
        if idx < 0 or idx > 2 or not self._client or not self._ch[idx]:
            return None
        try:
            data = await self._client.read_gatt_char(self._ch[idx])
            if len(data) >= 2:
                return data[0], data[1]
        except Exception as e:
            log.error(f"Read btn{idx+1} failed: {e}")
        return None

    async def write_button_mapping(self, idx: int, vk: int, mod: int) -> bool:
        if idx < 0 or idx > 2 or not self._client or not self._ch[idx]:
            return False
        try:
            await self._client.write_gatt_char(self._ch[idx], bytes([vk, mod]), response=True)
            return True
        except Exception as e:
            log.error(f"Write btn{idx+1} failed: {e}")
            return False

    async def read_tx_power(self) -> int | None:
        if not self._client or not self._ch_tx_power:
            return None
        try:
            data = await self._client.read_gatt_char(self._ch_tx_power)
            if len(data) >= 1:
                return data[0]
        except Exception as e:
            log.error(f"Read TX power failed: {e}")
        return None

    async def write_tx_power(self, level: int) -> bool:
        if not self._client or not self._ch_tx_power:
            return False
        try:
            await self._client.write_gatt_char(self._ch_tx_power, bytes([level]), response=True)
            return True
        except Exception as e:
            log.error(f"Write TX power failed: {e}")
            return False

    async def read_sleep_mode(self) -> int | None:
        if not self._client or not self._ch_sleep_mode:
            return None
        try:
            data = await self._client.read_gatt_char(self._ch_sleep_mode)
            if len(data) >= 1:
                return data[0]
        except Exception as e:
            log.error(f"Read sleep mode failed: {e}")
        return None

    async def write_sleep_mode(self, enabled: int) -> bool:
        if not self._client or not self._ch_sleep_mode:
            return False
        try:
            await self._client.write_gatt_char(self._ch_sleep_mode, bytes([enabled]), response=True)
            return True
        except Exception as e:
            log.error(f"Write sleep mode failed: {e}")
            return False

    def _on_button_event(self, _sender, data: bytearray):
        if len(data) >= 2 and self.on_button_event:
            self.on_button_event(data[0], data[1])

    def _on_status(self, _sender, data: bytearray):
        if len(data) >= 2 and self.on_status:
            self.on_status(data[0], data[1])
