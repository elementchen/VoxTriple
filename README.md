# VoxTriple — ESP32 三键蓝牙 PTT 麦克风

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange.svg)](https://platformio.org/)
[![.NET](https://img.shields.io/badge/app-.NET%208-purple.svg)](https://dotnet.microsoft.com/)

一个基于 ESP32 的蓝牙麦克风系统。三键物理按钮，一键 PTT 通话 + 可编程键盘快捷键。专为语音输入（Windows 语音识别 / 讯飞）设计。

## 功能特性

- **蓝牙麦克风** — ESP32 作为 HFP Hands-Free Client，Windows 原生识别为蓝牙音频输入设备
- **PTT 按住说话** — Button 1 模拟对讲机，按住采集音频、松开关闭，搭配 Windows 语音输入法使用
- **可编程快捷键** — 3 个按钮映射任意键盘组合键（含修饰键 Ctrl/Shift/Alt/Win）
- **BLE 无线配置** — Windows WPF 应用通过 BLE GATT 实时读写按钮映射
- **开机自动连接** — 首次配对后，Windows 应用启动自动连接 BLE，经典蓝牙自动重连
- **mSBC 宽带语音** — 16kHz 采样、硬件降噪 DSP（高通 + 滑动平均）

## 硬件清单

| 组件 | 型号 | 数量 |
|------|------|------|
| 开发板 | NodeMCU-32S (ESP32-WROOM-32) | 1 |
| 麦克风 | INMP441 全向 MEMS I2S 模块 | 1 |
| 按钮 | 6×6mm 轻触开关 | 3 |
| 电容 | 100nF 瓷片 (可选，硬件降噪) | 1 |

## 接线

```
INMP441 → ESP32:
  VDD  → 3.3V          （不可接 5V！）
  GND  → GND
  L/R  → GND           （选左声道）
  SD   → GPIO 32       （I2S 数据）
  WS   → GPIO 25       （I2S 字选择）
  SCK  → GPIO 26       （I2S 时钟）

Button 1 → GPIO 13     （PTT + 快捷键）
Button 2 → GPIO 12     （快捷键）
Button 3 → GPIO 14     （快捷键）

按钮接法：GPIO ↔ 按键 ↔ GND（低电平有效，内部上拉）
```

## 快速开始

### 1. 编译烧录固件

```bash
pip install platformio
cd esp32_bt_mic
pio run -t upload --upload-port COM4
```

### 2. 蓝牙配对

Windows 蓝牙设置 → 添加设备 → 搜索 `ESP32_BT_MIC` → 配对（PIN: `1234`）

### 3. 启动配置应用

```bash
cd windows_app
dotnet run --project Esp32BtMicConfig
```

### 4. 使用

1. 应用启动后自动连接 BLE，读取当前按钮映射
2. `Capture Key` 捕获键盘按键 → 勾选修饰键（Ctrl/Shift 等）→ `Write to Device`
3. 按住 Button 1 说话（PTT），松开停止
4. Button 2/3 触发已配置的键盘快捷键

## 项目结构

```
VoxTriple/
├── esp32_bt_mic/              # ESP32 固件 (PlatformIO + ESP-IDF 5.5)
│   ├── platformio.ini
│   ├── sdkconfig.defaults     # BTDM 双模 + HFP HF + BLE
│   ├── partitions.csv
│   └── src/
│       ├── main.c             # 入口
│       ├── bt_init.c/h        # BT 初始化 + GAP
│       ├── bt_hfp_hf.c/h      # HFP HF Client + SCO 音频管线
│       ├── bt_app_core.c/h    # BT 任务分发
│       ├── bt_app_hf.c/h      # HFP 回调
│       ├── ble_gatts_config.c/h # BLE GATT 服务 (0x1820)
│       ├── audio_capture.c/h  # I2S INMP441 驱动 (legacy driver)
│       ├── button_handler.c/h # 按钮检测 + 消抖
│       └── config_storage.c/h # NVS 配置存储
│
├── windows_app/               # Windows 配置应用 (.NET 8 WPF)
│   └── Esp32BtMicConfig/
│       ├── Services/
│       │   ├── BleGattClient.cs       # BLE 扫描/连接/读写
│       │   ├── KeyboardSimulator.cs   # keybd_event 键盘模拟
│       │   └── ConfigurationService.cs # JSON 配置持久化
│       ├── ViewModels/MainViewModel.cs # MVVM
│       └── Views/MainWindow.xaml/cs    # 主界面 + Win32 按键捕获
│
└── docs/
    ├── architecture.md         # 系统架构
    ├── ble_protocol.md         # BLE GATT 协议规范
    └── wiring_diagram.txt     # 详细接线图
```

## BLE 协议

- **服务 UUID**: `0x1820` (00001820-0000-1000-8000-00805F9B34FB)
- **Button 1-3 Map** (0x2A01-0x2A03): R/W, `[vk_code:u8, modifier:u8]`
- **Button Event** (0x2A04): Notify, `[button_id:u8, state:u8]`
- **Device Status** (0x2A05): Notify, `[hfp_connected:u8, audio_active:u8]`

修饰键位掩码：bit0=LCtrl, bit1=LShift, bit2=LAlt, bit3=LWin, bit4=RCtrl, bit5=RShift, bit6=RAlt, bit7=RWin

## 技术栈

| 组件 | 技术 |
|------|------|
| 微控制器 | ESP32 (Xtensa LX6, 240MHz) |
| 固件框架 | ESP-IDF 5.5 / PlatformIO |
| 蓝牙协议栈 | Bluedroid BTDM (Classic + BLE) |
| 音频编解码 | HFP mSBC 16kHz WBS |
| 麦克风驱动 | I2S Legacy Driver |
| 桌面应用 | .NET 8 WPF + CommunityToolkit.Mvvm |
| 键盘模拟 | Win32 `keybd_event` API |
| BLE 扫描 | `BluetoothLEAdvertisementWatcher` |

## 许可证

MIT License © 2024-2026

## 致谢

本项目在开发过程中解决了大量 ESP32 蓝牙双模的边界条件问题，包括 BTDM SCO 同步连接配置、BLE/经典蓝牙共存调度、I2S 驱动选择等。感谢 Espressif ESP-IDF 团队和开源社区的支持。
