# VoxTriple — ESP32 3-Button Bluetooth PTT Microphone

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange.svg)](https://platformio.org/)
[![.NET](https://img.shields.io/badge/app-.NET%208-purple.svg)](https://dotnet.microsoft.com/)

An ESP32-based Bluetooth microphone system with three physical buttons: one PTT (push-to-talk) button plus two programmable keyboard shortcuts. Designed for Windows voice input (Windows Speech Recognition / Voice Typing).

----

# VoxTriple — ESP32 三键蓝牙 PTT 麦克风

基于 ESP32 的蓝牙麦克风系统。三键物理按钮：一键 PTT 通话 + 两颗可编程键盘快捷键。专为 Windows 语音输入设计。

----

## Features / 功能特性

- **Bluetooth Microphone** — ESP32 as HFP Hands-Free Client. Windows recognizes it as a native Bluetooth audio input device.
- **PTT (Push-to-Talk)** — Button 1 acts like a walkie-talkie. Hold to capture audio, release to mute.
- **Programmable Shortcuts** — All 3 buttons can be mapped to any keyboard key + modifier combination (Ctrl / Shift / Alt / Win).
- **BLE Wireless Config** — Windows WPF app reads/writes button mappings via BLE GATT in real time.
- **Auto-start & Auto-connect** — BLE auto-reconnects on app launch. Classic Bluetooth reconnects automatically after pairing. Optional startup with Windows.
- **mSBC Wideband Speech** — 16kHz sampling with hardware noise-reduction DSP (high-pass filter + moving average).

## Hardware / 硬件

| Component 组件 | Model 型号 | Qty |
|---------------|-----------|-----|
| Dev Board     | NodeMCU-32S (ESP32-WROOM-32) | 1 |
| Microphone    | INMP441 MEMS I2S | 1 |
| Buttons       | 6×6mm tactile switch | 3 |
| Capacitor (optional) | 100nF ceramic (hardware noise reduction) | 1 |

## Wiring / 接线

```
INMP441 → ESP32:
  VDD  → 3.3V          (never 5V! / 不可接 5V)
  GND  → GND
  L/R  → GND           (left channel / 左声道)
  SD   → GPIO 32       (I2S data)
  WS   → GPIO 25       (word select)
  SCK  → GPIO 26       (bit clock)

Button 1 → GPIO 13     (PTT + shortcut / 快捷键)
Button 2 → GPIO 12     (shortcut)
Button 3 → GPIO 14     (shortcut)

Wired active-low with internal pull-up / 低电平有效，内部上拉
```

## Quick Start / 快速开始

### 1. Build & Flash Firmware / 编译烧录

```bash
pip install platformio
cd esp32_bt_mic
pio run -t upload --upload-port COM4
```

### 2. Pair Bluetooth / 蓝牙配对

Windows Settings → Bluetooth → Add device → `ESP32_BT_MIC` → Pair (PIN: `1234`)

### 3. Run Config App / 启动配置应用

```bash
cd windows_app
dotnet run --project Esp32BtMicConfig
```

Or use the single-file executable / 或直接运行单文件 EXE:

```bash
dotnet publish -r win-x64 -c Release -p:PublishSingleFile=true --self-contained false -o publish
# Run: publish/Esp32BtMicConfig.exe
```

### 4. Usage / 使用

1. App auto-connects BLE and reads current button mappings / 应用自动连接 BLE 并读取当前按钮映射
2. `Capture Key` → press a key → check modifiers → `Write to Device` / 捕获按键 → 勾选修饰键 → 写入设备
3. Hold Button 1 to speak (PTT), release to stop / 按住 Button 1 说话，松开停止
4. Button 2/3 trigger configured keyboard shortcuts / Button 2/3 触发已配置的快捷键

## Project Structure / 项目结构

```
VoxTriple/
├── esp32_bt_mic/              # ESP32 firmware (PlatformIO + ESP-IDF 5.5)
│   ├── platformio.ini
│   ├── sdkconfig.defaults     # BTDM dual-mode + HFP HF Client + BLE
│   ├── partitions.csv
│   └── src/
│       ├── main.c
│       ├── bt_init.c/h        # BT init + GAP
│       ├── bt_hfp_hf.c/h      # HFP HF Client + SCO audio pipeline
│       ├── bt_app_core.c/h    # BT task dispatcher
│       ├── bt_app_hf.c/h      # HFP callback
│       ├── ble_gatts_config.c/h # BLE GATT service (0x1820)
│       ├── audio_capture.c/h  # I2S INMP441 (legacy driver)
│       ├── button_handler.c/h # button debounce + PTT
│       └── config_storage.c/h # NVS config storage
│
├── windows_app/               # Windows config app (.NET 8 WPF)
│   └── Esp32BtMicConfig/
│       ├── Services/
│       │   ├── BleGattClient.cs         # BLE scan / connect / read / write
│       │   ├── KeyboardSimulator.cs     # keybd_event keyboard simulation
│       │   └── ConfigurationService.cs  # JSON config persistence
│       ├── ViewModels/MainViewModel.cs  # MVVM
│       └── Views/MainWindow.xaml/cs     # UI + Win32 key capture hook
│
└── docs/
    ├── architecture.md         # system architecture / 系统架构
    ├── ble_protocol.md         # BLE GATT protocol spec
    └── wiring_diagram.txt      # detailed wiring diagram / 详细接线图
```

## BLE Protocol / BLE 协议

- **Service UUID**: `0x1820` (00001820-0000-1000-8000-00805F9B34FB)
- **Button 1-3 Map** (0x2A01-0x2A03): R/W, `[vk_code:u8, modifier:u8]`
- **Button Event** (0x2A04): Notify, `[button_id:u8, state:u8]`
- **Device Status** (0x2A05): Notify, `[hfp_connected:u8, audio_active:u8]`

Modifier bitmask / 修饰键位掩码:

| Bit | Key |
|-----|-----|
| 0 | LCtrl |
| 1 | LShift |
| 2 | LAlt |
| 3 | LWin |
| 4 | RCtrl |
| 5 | RShift |
| 6 | RAlt |
| 7 | RWin |

## Tech Stack / 技术栈

| Component 组件 | Technology |
|---------------|------------|
| MCU | ESP32 (Xtensa LX6, 240MHz) |
| Firmware Framework | ESP-IDF 5.5 / PlatformIO |
| BT Stack | Bluedroid BTDM (Classic + BLE) |
| Audio Codec | HFP mSBC 16kHz WBS |
| Mic Driver | I2S Legacy Driver |
| Desktop App | .NET 8 WPF + CommunityToolkit.Mvvm |
| Keyboard Simulation | Win32 `keybd_event` |
| BLE Scanning | `BluetoothLEAdvertisementWatcher` |

## License / 许可证

MIT License © 2024-2026
