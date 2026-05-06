# ESP32 蓝牙麦克风项目

一个基于ESP32的蓝牙麦克风系统，支持蓝牙音频传输和快捷键功能。

## 功能特性

- **蓝牙麦克风**: ESP32通过HFP AG Profile连接Windows，作为蓝牙麦克风使用
- **快捷键功能**: 三个物理按钮可配置为任意键盘快捷键
- **配置应用**: Windows WPF应用程序用于配置按钮映射
- **双模蓝牙**: 同时支持经典蓝牙(HFP)和BLE(GATT)

## 硬件要求

- **开发板**: NodeMCU-32S (ESP32-WROOM-32)
- **麦克风**: INMP441全向麦克风模块
- **按钮**: 3个轻触按钮

### 接线图

```
ESP32 Pin    INMP441 Pin    功能
---------   ------------   ----
3.3V        VDD            电源
GND         GND            地
GPIO 32     SD             I2S数据输入
GPIO 25     WS             I2S字选择
GPIO 26     SCK            I2S时钟
GND         L/R            左声道

ESP32 Pin    按钮           功能
---------   --------       ----
GPIO 13     Button 1       快捷键1
GPIO 12     Button 2       快捷键2
GPIO 14     Button 3       快捷键3
```

详细接线图请参考 [docs/wiring_diagram.txt](docs/wiring_diagram.txt)

## 项目结构

```
ESP32_MIC_Claudecode/
├── esp32_bt_mic/           # ESP32固件 (PlatformIO + ESP-IDF)
│   ├── platformio.ini
│   ├── sdkconfig.defaults
│   └── main/               # 源代码
│
├── windows_app/            # Windows配置应用 (C# WPF)
│   └── Esp32BtMicConfig/
│
└── docs/                   # 项目文档
    ├── architecture.md     # 系统架构
    ├── ble_protocol.md     # BLE协议规范
    └── wiring_diagram.txt  # 接线图
```

## 快速开始

### 1. 编译ESP32固件

```bash
# 安装PlatformIO
pip install platformio

# 进入固件目录
cd esp32_bt_mic

# 编译固件
pio run

# 烧录固件
pio run -t upload

# 查看串口日志
pio device monitor
```

### 2. 构建Windows应用

```bash
# 进入应用目录
cd windows_app

# 还原依赖
dotnet restore

# 构建应用
dotnet build

# 运行应用
dotnet run --project Esp32BtMicConfig
```

### 3. 使用步骤

1. **烧录固件**: 将编译好的固件烧录到ESP32开发板
2. **连接设备**: 在Windows蓝牙设置中配对ESP32_BT_MIC设备
3. **启动应用**: 运行Windows配置应用
4. **配置按钮**: 在应用中设置三个按钮的快捷键映射
5. **使用麦克风**: ESP32会自动作为蓝牙麦克风出现在Windows音频设备中

## 技术栈

### ESP32固件
- **框架**: ESP-IDF v5.5.1
- **开发工具**: PlatformIO
- **蓝牙**: Bluedroid (HFP AG + BLE GATT)
- **音频**: I2S标准模式 (INMP441)

### Windows应用
- **框架**: .NET 8 WPF
- **MVVM**: CommunityToolkit.Mvvm
- **蓝牙**: Windows.Devices.Bluetooth (BLE GATT)
- **键盘模拟**: Win32 SendInput API

## BLE协议

详细的BLE GATT协议规范请参考 [docs/ble_protocol.md](docs/ble_protocol.md)

### 服务UUID
- 服务: `0x1820` (ESP32 Button Config Service)

### 特征值
- Button 1 Map: `0x2A01` (读/写)
- Button 2 Map: `0x2A02` (读/写)
- Button 3 Map: `0x2A03` (读/写)
- Button Event: `0x2A04` (读/通知)
- Device Status: `0x2A05` (读/通知)

## 常见问题

### Q: ESP32无法被Windows发现？
A: 确保ESP32已烧录固件并处于广播状态。在Windows蓝牙设置中点击"添加设备"。

### Q: 麦克风没有声音？
A: 检查INMP441接线是否正确，确保L/R引脚接地（左声道）。在Windows声音设置中选择ESP32_BT_MIC作为输入设备。

### Q: 按钮没有反应？
A: 检查按钮接线是否正确，确保按钮按下时GPIO接地。在配置应用中检查按钮映射设置。

### Q: BLE连接失败？
A: 确保ESP32和Windows设备在蓝牙范围内（10米以内）。尝试删除配对记录后重新配对。

## 开发说明

### ESP32固件开发

固件代码位于 `esp32_bt_mic/main/` 目录，主要模块：

- `main.c` - 入口点，初始化序列
- `bt_init.c` - 蓝牙初始化
- `bt_hfp_ag.c` - HFP AG Profile
- `ble_gatts_config.c` - BLE GATT服务
- `audio_capture.c` - I2S音频采集
- `button_handler.c` - 按钮检测
- `config_storage.c` - NVS配置存储

### Windows应用开发

应用代码位于 `windows_app/Esp32BtMicConfig/` 目录，主要模块：

- `Services/BleGattClient.cs` - BLE通信
- `Services/KeyboardSimulator.cs` - 键盘模拟
- `Services/ConfigurationService.cs` - 配置持久化
- `ViewModels/MainViewModel.cs` - 视图模型
- `Views/MainWindow.xaml` - 用户界面

## 许可证

MIT License

## 联系方式

如有问题或建议，请提交Issue。
