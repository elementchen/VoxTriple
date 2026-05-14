# Changelog

## v1.1-stable (2026-05-13)

> Simplified architecture: buttons only send BLE keyboard events, HFP audio fully managed by Windows AG. Zero task_wdt crashes.

### Firmware
- **Architecture simplified**: buttons no longer control SCO audio — only send BLE keyboard events
  - HFP pipeline always ready, Windows AG manages audio start/stop
  - Eliminates all BTU_TASK task_wdt deadlocks
- **BTU_TASK fix**: audio pipeline work deferred to app task via `bt_app_work_dispatch`
- **BLE fast connection**: request `conn_int=12-24` (15-30ms) for low-latency keyboard events
- **WS2812 rainbow restored**: 4 LEDs on GPIO 15, Button 1 press=start, release=stop
  - Hue limited to yellow-green-blue (60-240°), ping-pong gradient
  - Safe to run alongside Bluetooth — no RMT/BT conflict
- BLE connection interval forced to 15-30ms for low latency
- Audio send task priority lowered from 22 to 5

### Python App
- Keyboard simulation: `keybd_event` (pure VK codes, stable in all apps)
- BLE client: resolve characteristics by handle within service scope (fixes bleak UUID conflict)
- Key capture: pynput win32_event_filter, dispatched to tkinter main thread
- Single-file EXE: 11 MB PyInstaller output
- Auto-start on boot + auto-connect BLE on launch

### Docs
- README: full bilingual (English + Chinese)
- CHANGELOG.md: incremental version history
- Multi-device switching spec archived in docs/

### Windows C# App
- Archived to `_Archive/windows_app_csharp/`

----

## v1.0-stable (2026-05-10)

### Firmware
- **Simplified architecture**: buttons only send BLE keyboard events, no SCO control
  - HFP audio pipeline always ready, Windows AG manages SCO start/stop
  - Eliminates task_wdt crashes caused by SCO teardown in BTU_TASK
- **BTU_TASK starvation fix**: audio pipeline work deferred to app task via `bt_app_work_dispatch`
- **BLE fast connection**: request `conn_int=12-24` (15-30ms) for low-latency keyboard events
- **WS2812 rainbow restored**: 4 LEDs, Button 1 press=start, release=stop
  - Rainbow task safe now — no longer conflicts with SCO (buttons don't touch audio)
- Audio send task priority lowered from 22 to 5

### Python App
- **Keyboard simulation**: `keybd_event` (pure VK codes, no SCANCODE conflict)
- Windows C# app archived to `_Archive/`
- Python EXE rebuilt (11 MB)

----

## v1.0-stable (2026-05-10)

> Stable single-device release. All core features working reliably.

### Firmware
- HFP HF Client + mSBC 16kHz — Windows native Bluetooth microphone
- 3 buttons: configurable keyboard shortcuts via BLE GATT
- WS2812 LED strip (4 LEDs): rainbow on Button 1 press
- Legacy I2S driver for clean audio (INMP441)
- BLE GATT service 0x1820 with 5 characteristics
- BTDM dual-mode controller (SCO + BLE coexistence)
- Audio DSP: high-pass filter + moving average

### Windows App (C# WPF, archived)
- BLE scan (BluetoothLEAdvertisementWatcher), connect, GATT R/W
- Win32 key capture hook (bypasses IME, detects modifiers)
- keybd_event keyboard simulation
- Auto-start on boot, auto-connect BLE on launch

### Python App
- tkinter + bleak + pynput, independent alternative
- 11 MB PyInstaller single-file EXE

### Removed
- Multi-device switching (Button 4) — rolled back, spec archived in docs/

----

## 2026-05-10

### Multi-Device Switching & LED

- **新增 Button 4** (GPIO 27)：设备切换 + 配对管理
  - 短按 (<5s)：切换到下一个已配对设备，绿灯闪烁指示当前设备号
  - 长按 (≥5s, 不松手即触发)：清除当前设备配对记录并进入配对模式
  - 设备指示：1 次绿灯闪烁 = 设备 1 / 2 次 = 设备 2，各重复 3 轮
- **配对模式重写**
  - 蓝灯真正快闪（独立 FreeRTOS task），不再常亮
  - 非阻塞状态机，配对中可短按 Button 4 取消
  - 30s 超时蓝灯熄灭退出，不再亮红灯
  - HFP 连接成功自动保存设备地址到 NVS
- **PTT 增强**：BLE 未连接时按下 Button 1，红灯闪烁 3 次
- 新增 `config_storage` 设备列表 CRUD 函数，支持 2 设备 NVS 存储

### WS2812 LED

- 新增 `ws2812_device_indicator(dev_idx)`：N 次绿灯闪烁表示设备号
- 新增 `ws2812_blink_color()`：真正快闪（blink task）
- 修复 `ws2812_solid_color()` 不自动熄灭的 bug

----

## 2026-05-09

### Windows App — Key Capture & BLE Fixes

- **按键捕获改用 Win32 WM_KEYDOWN 钩子**（绕过 IME）
  - 捕获时临时关闭输入法，解决中文输入法 VK-E5 问题
  - Right Shift 改用扫描码 (0x36) 区分左右
  - Tab 键可捕获（`KeyboardNavigation.TabNavigation="None"` + `WM_GETDLGCODE`）
- **键盘模拟改用 `keybd_event`**（WiFi 项目验证方案），替代 `SendInput`
- **修饰键正确捕获**：左/右 Ctrl/Shift/Alt 自动区分（extended flag）
- **BLE 连接修复**
  - 扫描改用 `BluetoothLEAdvertisementWatcher` Active 模式
  - 连接改用 `BluetoothLEDevice.FromBluetoothAddressAsync()` 直连 MAC 地址
  - GATT 发现增加 3 次重试 + `BluetoothCacheMode.Uncached`
  - 连接后等待 1.5s 等服务注册完成
- **开机自动启动**：勾选框写入 Windows Startup 文件夹
- **开机自动连接 BLE**：首次 Scan & Connect 后记住地址，下次启动自动连
- **单文件 EXE 发布**：`dotnet publish -p:PublishSingleFile=true` → 25MB 单文件

### ESP32 固件 — 音质 + BLE 修复

- **I2S 驱动切换**：从新版 `i2s_std.h` 切换到旧版 `driver/i2s.h`
  - `use_apll = false`、`I2S_COMM_FORMAT_STAND_I2S`、`i2s_zero_dma_buffer()`
  - 音质大幅改善，消除电流噪音
- **I2S DMA 保持活跃**：SCO 断开时不真正停 I2S，解决二次连接挂死
- **BLE GATT 修复**
  - 广播数据拆分：广告包 (name+flags ≤31B) + 扫描响应包 (128-bit UUID)
  - 新增 `esp_ble_gatts_start_service()` 调用，修复服务不可见 bug
  - `GATTS_NUM_HANDLES` 12 → 16
  - Button Event 新增 CCCD 描述符
  - 修复 `s_conn_id == 0` 误判无连接 bug

### 音频降噪 DSP

- **高通滤波 (80Hz cutoff, 1-pole IIR)**：去除低频风噪、电路哼声
- **5-tap 滑动平均**：平滑高频数字毛刺
- 噪声门（默认禁用）

----

## 2026-05-07 — 2026-05-08

### HFP HF Client 重构

- **从 HFP AG 切换到 HFP HF Client**：Windows 正确识别为蓝牙麦克风
- BTDM SCO 致命配置修复：`CONFIG_BTDM_CTRL_BR_EDR_MAX_SYNC_CONN=1` 和 `SCO_DATA_PATH_HCI=y`
- CoD 更新：`service=0x340, major=0x04(Audio/Video), minor=0x02(Hands-free)`
- mSBC 16kHz 宽带语音编解码协商和音频管线
- **PTT (Button 1)**：按住开 SCO 音频、松开关闭
- I2S 音频路由优化：避免双重读取、ring buffer 9.6KB
- 出站回调欠载时返回静音而非 0，解决"哒哒哒"爆音

### Windows C# WPF 应用

- .NET 8 WPF + CommunityToolkit.Mvvm
- BLE GATT 客户端：扫描、连接、读写按钮映射
- KeyboardSimulator (Win32 SendInput)
- MVVM 架构 + JSON 配置持久化

### BLE GATT 服务

- 自定义服务 0x1820 + 5 个特征值
- Button 1-3 Map、Button Event (Notify)、Device Status (Notify)
- 广播数据 flags + 128-bit UUID + device name

### WS2812 LED

- GPIO 15, RMT 驱动, 15 颗灯珠
- PTT 按下 + BLE 连接时七彩循环

### Python tkinter 应用

- `windows_app_python/` 独立 Python 版本
- bleak BLE + pynput 按键捕获 + keybd_event 模拟
- tkinter GUI，11MB PyInstaller 单文件 EXE

### 文档

- GitHub README 中英双语
- MIT LICENSE
- 项目架构文档 + BLE 协议规范 + 接线图

----

## 2026-05-06 及之前

### 项目初始化

- ESP32 PlatformIO + ESP-IDF 项目骨架
- HFP AG 初始实现（后废弃）
- Windows WPF 应用初始版本
- 硬件接线文档
