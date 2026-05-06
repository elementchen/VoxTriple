# ESP32 蓝牙麦克风项目 - BLE GATT协议规范

## 概述

本文档定义了ESP32蓝牙麦克风项目的BLE GATT协议规范，用于Windows配置应用与ESP32设备之间的通信。

## 服务定义

### ESP32 Button Config Service

- **服务UUID**: `0x1820` (16-bit UUID)
- **完整UUID**: `00001820-0000-1000-8000-00805f9b34fb`
- **描述**: 按钮配置服务，用于读写按钮映射和接收按钮事件

## 特征值定义

### 1. Button 1 Map (按钮1映射)

| 属性 | 值 |
|------|-----|
| UUID | `0x2A01` |
| 属性 | 读, 写 |
| 格式 | `[vk_code:u8, modifier:u8]` |
| 描述 | 按钮1的键盘映射配置 |

**数据格式**:
- Byte 0: `vk_code` - Windows虚拟键码 (VK_*)
- Byte 1: `modifier` - 修饰键位掩码

**示例**:
- `[0x56, 0x01]` = Ctrl+V (粘贴)
- `[0x43, 0x01]` = Ctrl+C (复制)

### 2. Button 2 Map (按钮2映射)

| 属性 | 值 |
|------|-----|
| UUID | `0x2A02` |
| 属性 | 读, 写 |
| 格式 | `[vk_code:u8, modifier:u8]` |
| 描述 | 按钮2的键盘映射配置 |

### 3. Button 3 Map (按钮3映射)

| 属性 | 值 |
|------|-----|
| UUID | `0x2A03` |
| 属性 | 读, 写 |
| 格式 | `[vk_code:u8, modifier:u8]` |
| 描述 | 按钮3的键盘映射配置 |

### 4. Button Event (按钮事件)

| 属性 | 值 |
|------|-----|
| UUID | `0x2A04` |
| 属性 | 读, 通知 |
| 格式 | `[button_id:u8, state:u8]` |
| 描述 | 按钮按下/释放事件通知 |

**数据格式**:
- Byte 0: `button_id` - 按钮编号 (1, 2, 或 3)
- Byte 1: `state` - 状态 (0x00=释放, 0x01=按下)

**通知触发**:
- 按钮按下时发送通知
- 按钮释放时发送通知

### 5. Device Status (设备状态)

| 属性 | 值 |
|------|-----|
| UUID | `0x2A05` |
| 属性 | 读, 通知 |
| 格式 | `[hfp_connected:u8, audio_active:u8]` |
| 描述 | 设备状态信息 |

**数据格式**:
- Byte 0: `hfp_connected` - HFP连接状态 (0x00=断开, 0x01=已连接)
- Byte 1: `audio_active` - 音频活跃状态 (0x00=非活跃, 0x01=活跃)

## 修饰键位掩码

| Bit | 修饰键 | VK代码 | 描述 |
|-----|--------|--------|------|
| 0 | Left Ctrl | 0xA2 | 左Ctrl键 |
| 1 | Left Shift | 0xA0 | 左Shift键 |
| 2 | Left Alt | 0xA4 | 左Alt键 |
| 3 | Left Win | 0x5B | 左Win键 |
| 4 | Right Ctrl | 0xA3 | 右Ctrl键 |
| 5 | Right Shift | 0xA1 | 右Shift键 |
| 6 | Right Alt | 0xA5 | 右Alt键 |
| 7 | Right Win | 0x5C | 右Win键 |

**组合示例**:
- `0x01` = Ctrl
- `0x02` = Shift
- `0x03` = Ctrl+Shift
- `0x05` = Ctrl+Alt
- `0x09` = Ctrl+Win

## 常用虚拟键码

### 字母键
| VK码 | 按键 | VK码 | 按键 |
|------|------|------|------|
| 0x41 | A | 0x4E | N |
| 0x42 | B | 0x4F | O |
| 0x43 | C | 0x50 | P |
| 0x44 | D | 0x51 | Q |
| 0x45 | E | 0x52 | R |
| 0x46 | F | 0x53 | S |
| 0x47 | G | 0x54 | T |
| 0x48 | H | 0x55 | U |
| 0x49 | I | 0x56 | V |
| 0x4A | J | 0x57 | W |
| 0x4B | K | 0x58 | X |
| 0x4C | L | 0x59 | Y |
| 0x4D | M | 0x5A | Z |

### 功能键
| VK码 | 按键 | VK码 | 按键 |
|------|------|------|------|
| 0x70 | F1 | 0x7A | F11 |
| 0x71 | F2 | 0x7B | F12 |
| 0x72 | F3 | 0x20 | Space |
| 0x73 | F4 | 0x0D | Enter |
| 0x74 | F5 | 0x1B | Escape |
| 0x75 | F6 | 0x09 | Tab |
| 0x76 | F7 | 0x08 | Backspace |
| 0x77 | F8 | 0x2E | Delete |
| 0x78 | F9 | 0x2D | Insert |
| 0x79 | F10 | 0x21 | Page Up |

### 常用快捷键组合
| 快捷键 | 按钮1配置 | 按钮2配置 | 按钮3配置 |
|--------|-----------|-----------|-----------|
| Ctrl+C | [0x43, 0x01] | - | - |
| Ctrl+V | [0x56, 0x01] | - | - |
| Ctrl+Z | [0x5A, 0x01] | - | - |
| Ctrl+X | [0x58, 0x01] | - | - |
| Ctrl+A | [0x41, 0x01] | - | - |
| Alt+Tab | - | [0x09, 0x04] | - |
| Alt+F4 | - | [0x73, 0x04] | - |
| Win+L | - | - | [0x4C, 0x08] |

## 通信流程

### 1. 设备发现与连接

```
Windows App                          ESP32
    |                                   |
    |--- BLE Scan ------------------->|
    |<-- Advertisement (ESP32_BT_MIC)-|
    |                                   |
    |--- Connect ------------------->|
    |<-- Connection Established ------|
    |                                   |
    |--- Discover Services ---------->|
    |<-- Service 0x1820 Found --------|
    |                                   |
    |--- Discover Characteristics --->|
    |<-- Characteristics Found -------|
    |                                   |
    |--- Enable Notifications ------->|
    |<-- Notifications Enabled -------|
```

### 2. 读取按钮配置

```
Windows App                          ESP32
    |                                   |
    |--- Read Button 1 Map ---------->|
    |<-- [vk_code, modifier] ---------|
    |                                   |
    |--- Read Button 2 Map ---------->|
    |<-- [vk_code, modifier] ---------|
    |                                   |
    |--- Read Button 3 Map ---------->|
    |<-- [vk_code, modifier] ---------|
```

### 3. 写入按钮配置

```
Windows App                          ESP32
    |                                   |
    |--- Write Button 1 Map --------->|
    |    [button_id, vk_code, modifier]|
    |<-- Write Response --------------|
    |                                   |
    |--- Write Button 2 Map --------->|
    |    [button_id, vk_code, modifier]|
    |<-- Write Response --------------|
    |                                   |
    |--- Write Button 3 Map --------->|
    |    [button_id, vk_code, modifier]|
    |<-- Write Response --------------|
```

### 4. 按钮事件通知

```
Windows App                          ESP32
    |                                   |
    |                        [Button 1 Pressed]
    |<-- Notification ----------------|
    |    [button_id=1, state=0x01]    |
    |                                   |
    |                        [Button 1 Released]
    |<-- Notification ----------------|
    |    [button_id=1, state=0x00]    |
```

## 错误处理

### BLE错误码
| 错误码 | 描述 | 处理方式 |
|--------|------|----------|
| 0x01 | Invalid Handle | 重试连接 |
| 0x02 | Read Not Permitted | 检查特征值属性 |
| 0x03 | Write Not Permitted | 检查特征值属性 |
| 0x05 | Authentication | 需要配对 |
| 0x06 | Request Not Supported | 检查协议版本 |
| 0x0D | Invalid Parameter | 检查数据格式 |
| 0x0E | Attribute Long | 使用长读写 |
| 0x0F | Insufficient Encryption | 需要加密连接 |
| 0x10 | Insufficient Encryption Key Size | 检查密钥长度 |

### 超时处理
- 连接超时：10秒
- 读写超时：5秒
- 通知超时：无（持续监听）

## 安全考虑

### 配对方式
- 使用LE Secure Connections
- 配对码：1234 (固定)
- 加密等级：Level 3

### 数据验证
- 按钮ID验证：仅接受1, 2, 3
- VK代码验证：仅接受有效范围 (0x00-0xFF)
- 修饰键验证：仅接受有效位组合

## 性能优化

### MTU协商
- 默认MTU：23字节
- 协商MTU：512字节（最大）
- 实际使用：2-3字节（按钮配置）

### 连接参数
- 最小连接间隔：7.5ms
- 最大连接间隔：15ms
- 从机延迟：0
- 监控超时：4秒

### 广播参数
- 广播间隔：96ms (0x060)
- 广播类型：ADV_IND (可连接可扫描)
- 广播数据：设备名称 + 服务UUID
