# VoxTriple Changelog

## master (稳定版)

### v2.1 — 2026-06-01
- BLE HID 键盘（不打开应用也能用按键）
- 深度睡眠：30 分钟无操作 → ~5µA，GPIO4 唤醒
- BLE 连接失败自动重试
- 设备名：`ESP32_KB_XX` / `ESP32_MIC_XX`
- 修饰键左右区分（VK→HID 修饰位映射）
- Python UI 精简（移除断开/保存/加载/开机自启）
- 麦克风引脚：BCK=21, WS=22, DATA=17

### v2.0 — 2026-05-30
- BLE HID 键盘稳定版，合并主线

### v1.7 — 2026-05-28
- 4 按键全部右侧排，LED=GPIO18，TX Power + Sleep Mode BLE 控制
- 3x 音频增益，WiFi 禁用

### v1.1 — 2026-05-14
- 初版：3 按键，BLE GATT 配置，Python 应用

---

## ble-hid-keyboard (开发分支)

### 当前开发中 (基于 v2.1)
- **BLE OTA 固件升级**（0x2A09 特性，Python 应用无线刷固件）
- **去除扫描逻辑**（系统蓝牙负责连接，应用只做配置）
- **去除按键 HFP 重连**（避免 Windows 配对弹窗）
- 尝试 Mac 跨平台 HFP 兼容 → 失败，已回退
- 尝试去启动 HFP 自动连接 → 跨平台冲突 → 已回退
