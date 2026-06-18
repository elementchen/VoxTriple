# BT 5.3 适配器 BLE+Classic BT 双模共存 — 踩坑记录

## 背景

ESP32 BTDM（双模）固件在 BT 4.0 适配器下：BLE 键盘 + Classic BT HFP 耳机可正常首次连接。
更换 BT 5.3 USB 适配器后，BLE 键盘出现"连接→断开→重连"的闪断循环（bond loop）。

## 核心问题（至今未解）

**SC_BOND 的 NVS bond 密钥在 ESP32 重启后无法被 Windows 重新匹配。**

- 首次连接（NVS 干净）：BLE 键盘配对成功，按键正常工作 ✅
- 重启 ESP32（NVS 有密钥）：bond loop → `remove bond, rsn 102` → `MIC failure (0x3d)` → `remove bond in flash` ❌
- 重启后连 NVS 擦除都无效：ESP32 虽干净了，但 **Windows 端仍保留旧密钥**，持续用旧 LTK 加密 → MIC 失败

## 尝试过的方案

### 1. BLE 随机地址（区别 BLE/Classic BT MAC）
- **目的**: 让 Windows 把 BLE 键盘和 Classic BT 耳机识别为两个设备
- **方法**: `BLE_ADDR_TYPE_RANDOM` + `esp_ble_gap_set_rand_addr()`
- **错误**: 初始版本把 byte[0] 设为 0xC0（应设 byte[5] 高两位 = 11）
- **修正**: `ble_addr[5] = 0xC0 | (pub[5] & 0x3F)` —— 静态随机地址格式正确
- **结果**: 首次连接 OK，重启仍 bond loop。**随机地址不是根因**

### 2. SC_ONLY（不做 bonding）
- **目的**: 不存 bond 密钥，每次全新配对
- **结果**: 首次连接 ~3 秒后 Windows 主动断开（`rsn 0x13`）
- **可能原因**: Windows 要求 HID 键盘做 bonding（安全策略），不信任无 bond 的设备

### 3. 传统配对 ESP_LE_AUTH_BOND（不用 Secure Connections）
- **目的**: 避免 SC 的数值比较（numeric comparison）超时
- **结果**: 同样 bond loop，无改善

### 4. 自定义 GATT UUID（16-bit → 128-bit）
- **目的**: 避免 SIG 保留 UUID（0x2A01-0x2A08）冲突，BT 5.3 合规检查
- **结果**: 无帮助。自定义服务禁用后连接从 3s 延长到 31s，说明有影响但非根因

### 5. 禁用 BLE_SMP（与 a2dp_gatts_coex 一致）
- **目的**: 只用 Classic BT SMP 处理所有加密，消除双 SMP 锁死
- **结果**: 加密不再 `BTM_SetEncryption busy`，但 3s 后仍断开

### 6. 启动时 NVS 擦除
- **目的**: 每次启动 = 干净密钥 = 全新配对
- **结果**: ESP32 侧干净，但 **Windows 保留旧密钥** → 仍用旧 LTK → MIC 失败
- **无法控制 Windows 端行为**

### 7. Classic BT HFP 自动连接禁用
- **目的**: 避免 BLE 和 Classic BT 同时建连 → BTDM 冲突 → `BTM_SetEncryption busy`
- **改动**: 去掉启动时 auto-connect、BLE 连接时 HFP 重连、按键时 HFP 重连
- **结果**: 首次连接更稳定，但重启 bond loop 依旧

### 8. Classic BT NON_DISCOVERABLE / NON_CONNECTABLE
- **目的**: 防止 Windows 发现 Classic BT 设备后自动连接造成 BTDM 冲突
- **结果**: 仍有 `BT_SDP` 错误（Windows 从历史配对中记住了设备），bond loop 依旧

### 9. 连接参数优化
- **目的**: 延长 supervision timeout，加 slave latency 减少 BTDM 压力
- **结果**: Windows 拒绝了我们的参数更新（status=16），用自己的参数

### 10. 断开重连逻辑优化
- **目的**: 静态定时器（不泄露）+ 更长延迟（10s/5s）
- **结果**: 减少崩溃，但不影响 bond loop

### 11. 按键冷却（30s）
- **目的**: 防止按键频繁触发 HFP/BLE 重连加剧 BTDM 冲突
- **结果**: 有帮助但不影响根因

### 12. 双 GATT 注册修复
- **目的**: HID 连接/断开事件正确追踪
- **结果**: 日志更清晰，不影响根因

## 未尝试的方向

1. **纯 BLE 模式**（禁用所有 Classic BT）：验证 BLE HID 键盘本身在 BT 5.3 下是否正常
2. **BLE-only controller 模式**（`CONFIG_BTDM_CTRL_MODE_BLE_ONLY`）：硬件层面避开 BTDM
3. **NimBLE 栈**（替代 Bluedroid）：BLE-only，无 Classic BT
4. **Windows 蓝牙缓存清理**：不同适配器有独立配对列表
5. **bond 密钥手动清除 API**：（`esp_ble_remove_bond_device`）延迟调用
