# 踩坑记录 — 2026-06-09 / 06-10

## 背景

用户更换了蓝牙适配器（4.0 → 5.3），BLE 连接开始不稳定。尝试在 6/6 稳定版基础上加新功能，但开发过程混乱导致问题叠加。

## 失败尝试

### 1. 反复切换 BLE 安全参数
**尝试**：`SC_BOND ↔ BOND ↔ SC_MITM_BOND`、`IO_CAP_NONE ↔ IO_CAP_IO`
**结果**：配对失败/闪断。问题在适配器不在代码。
**教训**：一旦某个安全参数组合稳定工作，**不要随意更换**。

### 2. 结构化广播 ↔ 原始广播
**尝试**：结构化 API（128位 UUID）↔ 手拼原始字节（16位 UUID）
**结果**：手拼版数据格式可能有问题，Windows 不识别为键盘
**教训**：用 ESP-IDF 的结构化 API，让栈处理字节拼装。

### 3. 在 BLE 连接时自动触发 HFP 重连
**问题**：`BTM_SetEncryption busy` → `earlier enc was not done` → 闪断
**教训**：BTDM 双模不能同时进行 BLE 和经典蓝牙的加密协商。**BLE 连接时不要自动触发 HFP 连接**——留给用户手动操作。

### 4. LED、组合键等功能叠加在 BLE 修复上
**问题**：无法分辨是 BLE 修复还是新功能导致的 bug
**教训**：**每次只改一个变量**，验证通过后再加下一个。

### 5. auto-save hook 干扰代码回退
**问题**：checkout 旧版本后 hook 又改回当前版本 → 编译出混合代码
**教训**：回退测试时用**新分支 + git stash**，确保工作区干净。

### 6. UART OTA 任务与串口控制台冲突
**问题**：`uart_read_bytes driver error`、`task_wdt triggered by uart_ota`
**教训**：不要和 ESP-IDF 控制台抢 UART0。UART OTA 需要独立 UART 或取消控制台。

### 7. BLE 随机地址格式
**问题**：`BLE_ADDR_TYPE_RANDOM` 需要先调用 `esp_ble_gap_set_rand_addr()` 设置静态随机地址
**教训**：静态随机地址首字节必须是 `0xC0`（bit7+bit6=1），后续5字节随机。

## 正确的做法

1. **基于 6/6 稳定版，每次只加一个改动，验证后再加下一个**
2. **BLE 随机地址**：唯一需要保留的新改动（解决 BT 5.3 同 MAC 设备混淆）
3. **麦克风开关**：第二个需要加的（解决不连耳机时避免 BTDM 冲突）
4. LED 反馈、组合键等功能等基础稳定后再加

## 2026-06-06 稳定版基线
- Commit: `5acaba4`
- BLE 安全: `ESP_LE_AUTH_REQ_SC_BOND` + `ESP_IO_CAP_NONE`
- 广播: 结构化 API + 128位 HID UUID
- 连接: BLE 连接后不自动触发 HFP
- BT 模式: 始终 BTDM（没有 BLE-only 选项）
