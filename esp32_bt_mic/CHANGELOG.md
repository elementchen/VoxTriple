# VoxTriple Changelog

## v2.2-ota (current, `ble-hid-keyboard` branch) | 2026-06-02

### 新增
- **BLE OTA 固件升级**：Python 应用通过 BLE 无线更新 ESP32 固件
- **直接地址连接**：应用启动时用已保存地址直连，无需扫描广播
- **广播保持**：任意 BLE 连接后自动恢复广播，允许多客户端

### 修复
- 修饰键 (Ctrl/Alt) 左右不分 → VK 码自动映射到 HID 修饰位
- Python 键盘模拟与 HID 双重输入导致连击 → 移除 Python 端模拟
- 广播地址随机变化导致重复设备 → 固定 BLE 公开地址
- TX Power Combobox IntVar 绑定失败 → Combobox.current()

---

## v2.1-deep-sleep | 2026-06-01

### 新增
- **深度睡眠**：30 分钟无按键 → 深睡（~5µA），GPIO4 唤醒
- **按键唤醒**：连接断开时按任意键触发重连
- **连接弹性**：BLE 连接失败 5 秒自动重试

### 改进
- BLE/经典蓝牙同时可用，任意顺序连接
- 设备名缩短：`ESP32_KB_XX` + `ESP32_MIC_XX`

---

## v2.0-ble-hid | 2026-05-30

### 新增
- **BLE HID 键盘**：不打开应用也能用按键，Windows 自动连接
- SC_BOND 安全配对，无需用户确认
- 耳机连接后自动触发键盘广播

---

## v1.7 | 2026-05-28

- 4 按键全部右侧排 (J2)
- BLE GATT: TX Power, Sleep Mode
- 3x 音频增益, WiFi 禁用

---

## v1.1 | 2026-05-14

- 初版：3 按键, BLE GATT 配置, Python 应用
