# VoxTriple — ESP32 蓝牙键盘 + 麦克风

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

基于 ESP32 的蓝牙设备，同时作为 **BLE 键盘**（4 键）和 **经典蓝牙 HFP 麦克风**。按键无需应用程序即可工作，Windows/macOS 自动识别。

## 功能

- **BLE HID 键盘**：4 个可编程按键，独立工作，不开应用也能用
- **经典蓝牙麦克风**：HFP 免提，Windows 原生音频输入设备
- **BLE GATT 配置**：Python 桌面应用无线配置按键映射、TX Power、Sleep Mode
- **固件升级**：USB 串口（12 秒）或 BLE（3 分钟）
- **深度睡眠**：30 分钟无操作 → ~5µA，GPIO4 唤醒

## 设备名

- BLE 键盘：`ESP32_KB_XX`
- 经典蓝牙 HFP：`ESP32_MIC_XX`

## 项目结构

```
├── esp32_bt_mic/          # ESP32 固件 (PlatformIO + ESP-IDF)
├── windows_app_python/     # Windows Python 配置应用
├── MAC_app_python/         # macOS Python 配置应用
├── _PCB/                   # PCB 设计文件
├── docs/                   # 技术文档
├── _Archive/               # 历史文件
└── CHANGELOG.md            # 版本记录
```

## 引脚 (v2.2)

全部右侧排 (J2)：BCK=21, WS=22, DATA=17, BTN1=4, BTN2=16, BTN3=19, BTN4=23, LED=18

详见 `docs/接线引脚V1.txt`

## 版本

| 分支 | 说明 |
|------|------|
| `master` | 稳定版 v2.1 |
| `ble-hid-keyboard` | 开发版 v2.2 (串口 OTA, 版本检测) |

## License

MIT
