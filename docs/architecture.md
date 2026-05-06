# ESP32 蓝牙麦克风项目 - 系统架构

## 概述

本项目实现了一个基于ESP32的蓝牙麦克风系统，包含两个主要组件：
1. **ESP32固件**：实现蓝牙麦克风功能和快捷键功能
2. **Windows配置应用**：用于配置按钮映射的键盘按键

## 系统架构图

```
┌─────────────────────────────────────────────────────────┐
│                    Windows PC                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ 蓝牙音频输入  │  │ WPF配置应用  │  │ 键盘模拟     │  │
│  │ (HFP AG)     │  │ (BLE GATT)   │  │ (SendInput)  │  │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │
│         │                 │                 │           │
│         └────────┬────────┴─────────────────┘           │
│                  │ Bluetooth Dual Mode                  │
└──────────────────┼──────────────────────────────────────┘
                   │
┌──────────────────┼──────────────────────────────────────┐
│                  │        ESP32                          │
│  ┌───────────────┴──────────────────────────────────┐   │
│  │           Bluetooth Stack (BTDM)                 │   │
│  │  ┌─────────────┐  ┌─────────────────────────┐   │   │
│  │  │   HFP AG    │  │    BLE GATT Server      │   │   │
│  │  │  (SCO音频)  │  │  (配置服务+按钮通知)    │   │   │
│  │  └──────┬──────┘  └───────────┬─────────────┘   │   │
│  └─────────┼─────────────────────┼─────────────────┘   │
│            │                     │                      │
│  ┌─────────┴─────────┐  ┌───────┴───────────────┐     │
│  │   Audio Capture   │  │   Button Handler      │     │
│  │  (I2S INMP441)    │  │  (GPIO 13/12/14)      │     │
│  └───────────────────┘  └───────────────────────┘     │
└─────────────────────────────────────────────────────────┘
```

## 蓝牙Profile说明

### HFP AG (Hands-Free Profile Audio Gateway)
- **用途**：ESP32作为蓝牙麦克风向Windows传输音频
- **音频格式**：
  - CVSD模式：8kHz, 16-bit, 单声道, 120字节/7.5ms帧
  - mSBC模式：16kHz, 16-bit, 单声道, 240字节/7.5ms帧
- **数据传输**：通过SCO同步链路传输原始PCM音频

### BLE GATT Server
- **用途**：配置按钮映射和接收按钮事件通知
- **服务UUID**：0x1820
- **特征值**：
  - Button 1-3 Map (0x2A01-0x2A03)：读/写，键盘映射配置
  - Button Event (0x2A04)：读/通知，按钮按下/释放事件
  - Device Status (0x2A05)：读/通知，设备状态

## 数据流

### 音频流
```
INMP441麦克风 (I2S)
    ↓
[I2S RX Task] -- 读取16-bit单声道PCM，16kHz
    ↓
[Audio Capture Task] -- 采样率转换（CVSD: 8kHz, mSBC: 16kHz）
    ↓
[HFP AG Outgoing Callback] -- 每7.5ms调用一次，从环形缓冲区读取
    ↓
[SCO Link] -- 原始PCM传输到Windows
    ↓
Windows蓝牙音频输入（识别为麦克风）
```

### 按钮事件流
```
[Button GPIO Task] -- GPIO 13/12/14消抖检测
    ↓
[BLE GATT Notify] -- 发送按钮事件到Windows
    ↓
[Windows WPF App] -- 接收通知，通过SendInput模拟键盘
```

### 配置流
```
[Windows WPF App] -- 通过BLE GATT写入按钮映射
    ↓
[ESP32 GATT Server] -- 接收配置，存储到NVS
    ↓
[Button Handler] -- 读取配置，执行相应的键盘快捷键
```

## 硬件连接

### 开发板信息
- **型号**: NodeMCU-32S
- **芯片**: ESP32-WROOM-32
- **USB转串口**: CP2102
- **板载LED**: GPIO 2

### INMP441麦克风连接

| ESP32引脚 | INMP441引脚 | 功能 |
|-----------|-------------|------|
| 3.3V | VDD | 电源 |
| GND | GND | 地 |
| GPIO 32 | SD | I2S数据输入 |
| GPIO 25 | WS | I2S字选择 |
| GPIO 26 | SCK | I2S时钟 |
| - | L/R | GND (左声道) |

### 按钮连接

| ESP32引脚 | 按钮 | 功能 |
|-----------|------|------|
| GPIO 13 | Button 1 | 快捷键1 |
| GPIO 12 | Button 2 | 快捷键2 |
| GPIO 14 | Button 3 | 快捷键3 |

## 软件架构

### ESP32固件模块

| 模块 | 职责 |
|------|------|
| main.c | 入口点，初始化序列 |
| bt_init.c | 蓝牙控制器+Bluedroid初始化 |
| bt_hfp_ag.c | HFP AG Profile，SCO音频 |
| ble_gatts_config.c | BLE GATT服务+特征值 |
| audio_capture.c | I2S INMP441驱动 |
| audio_task.c | 音频处理+HFP数据填充 |
| button_handler.c | GPIO按钮检测+消抖 |
| config_storage.c | NVS配置存储 |
| bt_app_core.c | 蓝牙任务+环形缓冲区 |
| bt_app_hf.c | HFP回调处理 |

### Windows应用模块

| 模块 | 职责 |
|------|------|
| BleGattClient.cs | BLE GATT客户端通信 |
| KeyboardSimulator.cs | Win32 SendInput键盘模拟 |
| ConfigurationService.cs | JSON配置持久化 |
| MainViewModel.cs | MVVM视图模型 |
| MainWindow.xaml | 用户界面 |

## 配置存储

### NVS (ESP32)
- 命名空间：`btn_config`
- 键：
  - `btn1_vk`, `btn1_mod`：按钮1的VK代码和修饰键
  - `btn2_vk`, `btn2_mod`：按钮2的VK代码和修饰键
  - `btn3_vk`, `btn3_mod`：按钮3的VK代码和修饰键

### JSON (Windows)
- 路径：`%AppData%/Esp32BtMicConfig/config.json`
- 内容：设备地址、按钮映射配置
