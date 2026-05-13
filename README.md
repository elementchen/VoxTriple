# VoxTriple — ESP32 3-Button Bluetooth PTT Microphone<br/>ESP32 三键蓝牙 PTT 麦克风

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange.svg)](https://platformio.org/)
[![.NET](https://img.shields.io/badge/app-.NET%208-purple.svg)](https://dotnet.microsoft.com/)

> An ESP32-based Bluetooth microphone system with three physical buttons: programmable keyboard shortcuts. Button 1 also drives a WS2812 rainbow LED strip. Designed for Windows voice input.
>
> 基于 ESP32 的蓝牙麦克风系统。三键物理按钮均可编程为键盘快捷键，Button 1 同时驱动 WS2812 彩虹灯带。专为 Windows 语音输入（语音识别 / 讯飞）设计。

----

## Features / 功能特性

- **Bluetooth Microphone** — ESP32 acts as an HFP Hands-Free Client. Windows recognizes it natively as a Bluetooth audio input device. No driver needed.
  
  **蓝牙麦克风** — ESP32 扮演 HFP Hands-Free Client 角色，Windows 原生识别为蓝牙音频输入设备，免驱即用。

- **Rainbow LED** — Button 1 lights a 4-LED WS2812 rainbow strip on GPIO 15. Press to start animation, release to stop.

  **彩虹灯带** — Button 1 驱动 4 颗 WS2812 彩虹灯珠（GPIO 15）。按住播放动画，松手熄灭。

- **Programmable Shortcuts** — All 3 buttons can be mapped to any Windows key combination including modifiers (Ctrl / Shift / Alt / Win). Configurable wirelessly.

  **可编程快捷键** — 3 个按钮均可映射任意键盘组合键（含 Ctrl/Shift/Alt/Win 修饰键），通过 BLE 无线配置。

- **BLE Wireless Config** — A companion Windows WPF app reads and writes button mappings over BLE GATT in real time. No need to reflash the ESP32 to change shortcuts.

  **BLE 无线配置** — Windows WPF 配置应用通过 BLE GATT 实时读写按钮映射。改快捷键不需要重新烧录 ESP32。

- **Auto-start & Auto-connect** — The Windows app optionally starts with Windows, then automatically scans for and reconnects BLE. Classic Bluetooth (HFP) reconnects automatically after the first pairing.

  **开机自动连接** — Windows 应用可设置为开机自启动，启动后自动扫描并连接 BLE。经典蓝牙（HFP）首次配对后自动重连。

- **mSBC Wideband Speech** — 16 kHz sampling with on-device noise-reduction DSP (high-pass filter + moving average). Clean voice input at telephone-grade quality.

  **mSBC 宽带语音** — 16kHz 采样，板载降噪 DSP（高通滤波 + 滑动平均）。提供电话级清晰度的语音输入。

- **Single-file EXE** — The Windows app can be published as a single standalone `.exe` with no DLL dependencies.

  **单文件 EXE** — Windows 应用可发布为单个 `.exe` 文件，无需附带任何 DLL。

## Hardware / 硬件清单

| Component 组件 | Model 型号 | Qty 数量 |
|---------------|-----------|---------|
| Dev Board 开发板 | NodeMCU-32S (ESP32-WROOM-32) | 1 |
| Microphone 麦克风 | INMP441 MEMS I2S module | 1 |
| Buttons 按钮 | 6×6mm tactile switch / 轻触开关 | 3 |
| Capacitor (optional) 电容(可选) | 100nF ceramic for hardware noise reduction / 瓷片电容用于硬件降噪 | 1 |

## Wiring / 接线

```
INMP441 → ESP32:
  VDD  → 3.3V          Never connect to 5V! / 不可接 5V！
  GND  → GND
  L/R  → GND           Left channel / 左声道
  SD   → GPIO 32       I2S data input / 数据输入
  WS   → GPIO 25       Word select / 字选择 (LRCLK)
  SCK  → GPIO 26       Bit clock / 位时钟 (BCLK)

Button 1 → GPIO 13     Keyboard shortcut + LED rainbow / 快捷键 + 彩虹灯
Button 2 → GPIO 12     Keyboard shortcut / 快捷键
Button 3 → GPIO 14     Keyboard shortcut / 快捷键

WS2812 → ESP32:
  DIN → GPIO 15         Data in / 数据输入
  VCC → 5V              External power (4 LEDs ≈ 240mA) / 外部供电
  GND → GND             Common ground / 共地

Buttons are wired active-low (GPIO → button → GND) with internal pull-up enabled.
按钮为低电平有效（GPIO → 按键 → GND），使用内部上拉电阻。
```

## Quick Start / 快速开始

### 1. Build & Flash Firmware / 编译烧录固件

```bash
pip install platformio
cd esp32_bt_mic
pio run -t upload --upload-port COM4
```

### 2. Pair Bluetooth / 蓝牙配对

Open Windows Bluetooth settings → Add device → `ESP32_BT_MIC` → enter PIN `1234`

打开 Windows 蓝牙设置 → 添加设备 → 搜索 `ESP32_BT_MIC` → 配对码 `1234`

### 3. Build & Run Config App / 编译运行配置应用

```bash
cd windows_app
dotnet run --project Esp32BtMicConfig
```

To produce a single-file `.exe` （打包为单文件 EXE）:

```bash
cd windows_app/Esp32BtMicConfig
dotnet publish -r win-x64 -c Release -p:PublishSingleFile=true --self-contained false -o publish
# Output / 输出: publish/Esp32BtMicConfig.exe (~25 MB)
```

### 4. Daily Use / 日常使用

1. The app auto-connects BLE on launch and reads the current button mappings from the ESP32.
   应用启动时自动连接 BLE 并读取 ESP32 上当前的按钮映射。
2. Click `Capture Key`, press any key on your keyboard, optionally check modifier boxes (Ctrl, Shift, etc.), then click `Write to Device`.
   点击 `Capture Key`，按下键盘上的任意按键，可选勾选修饰键（Ctrl/Shift 等），然后点击 `Write to Device`。
3. Hold **Button 1** to speak (PTT). The audio opens while you hold and closes when you release.
   按住 **Button 1** 说话（PTT），音频在按住期间打开，松开后关闭。
4. Press **Button 2** / **Button 3** to trigger their configured keyboard shortcuts.
   按下 **Button 2** / **Button 3** 触发各自配置的键盘快捷键。
5. Check `开机自动启动` in the app to have it launch automatically when Windows boots.
   勾选界面中的 `开机自动启动` 即可在 Windows 开机时自动启动应用。

## Project Structure / 项目结构

```
VoxTriple/
├── esp32_bt_mic/              # ESP32 firmware / ESP32 固件 (PlatformIO + ESP-IDF 5.5)
│   ├── platformio.ini
│   ├── sdkconfig.defaults     # BTDM dual-mode + HFP HF Client + BLE
│   ├── partitions.csv
│   └── src/
│       ├── main.c                       # Entry point / 入口
│       ├── bt_init.c/h                  # BT init + GAP / 蓝牙初始化
│       ├── bt_hfp_hf.c/h                # HFP HF Client + SCO audio pipeline
│       ├── bt_app_core.c/h              # BT task dispatcher / 蓝牙任务分发
│       ├── bt_app_hf.c/h                # HFP callback / HFP 回调
│       ├── ble_gatts_config.c/h         # BLE GATT service (0x1820) / BLE 服务
│       ├── audio_capture.c/h            # I2S INMP441 driver (legacy) / I2S 驱动
│       ├── button_handler.c/h           # Button debounce + PTT / 按钮消抖
│       └── config_storage.c/h           # NVS config storage / NVS 配置存储
│
├── windows_app/               # Windows config app / Windows 配置应用 (.NET 8 WPF)
│   └── Esp32BtMicConfig/
│       ├── Services/
│       │   ├── BleGattClient.cs         # BLE scan / connect / R/W / BLE 扫描连接读写
│       │   ├── KeyboardSimulator.cs     # keybd_event keyboard simulation / 键盘模拟
│       │   └── ConfigurationService.cs  # JSON config persistence / JSON 配置持久化
│       ├── ViewModels/MainViewModel.cs  # MVVM ViewModel
│       └── Views/MainWindow.xaml/cs     # UI + Win32 key capture hook / 主界面
│
└── docs/
    ├── architecture.md         # System architecture / 系统架构
    ├── ble_protocol.md         # BLE GATT protocol specification / BLE 协议规范
    └── wiring_diagram.txt      # Detailed wiring diagram / 详细接线图
```

## BLE Protocol / BLE 协议

The ESP32 exposes a custom GATT service for button configuration. The Windows app communicates with the ESP32 over BLE to read and write button mappings in real time.

ESP32 暴露出一个自定义 GATT 服务用于按钮配置。Windows 应用通过 BLE 与 ESP32 通信，实时读写按钮映射。

- **Service UUID / 服务 UUID**: `0x1820` (00001820-0000-1000-8000-00805F9B34FB)
- **Button 1-3 Map / 按钮 1-3 映射** (0x2A01-0x2A03): Read/Write, `[vk_code:u8, modifier:u8]`
  - `vk_code` — Windows Virtual-Key code (e.g. 0x0D = Enter, 0x09 = Tab) / Windows 虚拟键码
  - `modifier` — Modifier key bitmask (see below) / 修饰键位掩码（见下表）
- **Button Event / 按钮事件** (0x2A04): Notify, `[button_id:u8, state:u8]`
  - Sent whenever a physical button is pressed (state=1) or released (state=0) / 物理按钮按下(1)或松开(0)时发送
- **Device Status / 设备状态** (0x2A05): Notify, `[hfp_connected:u8, audio_active:u8]`

Modifier key bitmask / 修饰键位掩码:

| Bit | Key |
|-----|-----|
| 0 | Left Ctrl |
| 1 | Left Shift |
| 2 | Left Alt |
| 3 | Left Win |
| 4 | Right Ctrl |
| 5 | Right Shift |
| 6 | Right Alt |
| 7 | Right Win |

## Tech Stack / 技术栈

| Component 组件 | Technology |
|---------------|------------|
| MCU 芯片 | ESP32 (Xtensa LX6, 240 MHz) |
| Firmware Framework 固件框架 | ESP-IDF 5.5 / PlatformIO |
| Bluetooth Stack 蓝牙协议栈 | Bluedroid BTDM (Classic + BLE dual-mode) |
| Audio Codec 音频编解码 | HFP mSBC 16 kHz WBS (Wideband Speech) |
| Mic Driver 麦克风驱动 | I2S Legacy Driver (`driver/i2s.h`) |
| Desktop App 桌面应用 | .NET 8 WPF + CommunityToolkit.Mvvm |
| Keyboard Simulation 键盘模拟 | Win32 `keybd_event` API |
| BLE Scanning BLE 扫描 | `BluetoothLEAdvertisementWatcher` (Active mode) |

## License / 许可证

MIT License © 2024-2026
