# ESP32-S3 研究结论 & ESP-SR 降噪集成指南

> 写给接手本项目的 Agent：本文档记录本次 ESP32-S3 适配研究的关键发现，以及 ESP-SR（AI 音频前端）在实际项目中的集成方法。切换到 ESP32（原版）项目后可直接参考。

---

## 1. ESP32-S3 研究结论：Classic Bluetooth 不可用

### 核心发现

**ESP32-S3 不支持 Classic Bluetooth (BR/EDR)**，导致基于 HFP 的蓝牙麦克风方案无法在此芯片上运行。

### 证据链

**硬件层面** — ESP32-S3 SoC 能力定义（`components/soc/esp32s3/include/soc/soc_caps.h`）：

```c
#define SOC_BT_SUPPORTED                1
#define SOC_BLE_SUPPORTED               (1)    /* BLE */
#define SOC_BLE_50_SUPPORTED            (1)    /* Bluetooth 5.0 */
// 没有 SOC_BT_CLASSIC_SUPPORTED  ← 关键缺失
```

对比 ESP32 原版（`components/soc/esp32/include/soc/soc_caps.h`）：

```c
#define SOC_BT_SUPPORTED            1
#define SOC_BT_CLASSIC_SUPPORTED    (1)    /* ← 有这个 */
```

**Kconfig 层面** — `BT_CLASSIC_ENABLED` 的依赖链（`components/bt/host/bluedroid/Kconfig.in:53`）：

```
BT_CLASSIC_ENABLED depends on SOC_BT_CLASSIC_SUPPORTED
```

ESP32-S3 不存在 `SOC_BT_CLASSIC_SUPPORTED`，因此 Kconfig 直接隐藏了 Classic BT 所有选项。无论怎么修改 `sdkconfig.defaults` 都无法启用 — 这不是配置问题，是硬件不支持。

**官方文档** — ESP-IDF v5.3.1 Bluetooth Guide for ESP32-S3 明确声明：

> *"ESP-Bluedroid for ESP32-S3 supports **Bluetooth LE only**. Classic Bluetooth is **not supported**."*
>
> *"ESP-NimBLE supports **Bluetooth LE only**. Classic Bluetooth is **not supported**."*

— https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/api-guides/bluetooth.html

**文档 404** — ESP32-S3 的 Classic BT 文档页面返回 404，官方说明是"当前芯片不支持该功能"：
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/bluetooth/classic_bt.html

### 影响

| 功能 | ESP32 (原版) | ESP32-S3 |
|------|-------------|----------|
| Classic BT (BR/EDR) | ✓ | ✗ |
| BLE | ✓ | ✓ |
| HFP (Hands-Free Profile) | ✓ | ✗ |
| SCO/eSCO 音频通道 | ✓ | ✗ |
| mSBC 编解码器 | ✓ | ✗ |
| 作为 Windows 蓝牙麦克风 | ✓ | ✗ |
| ESP-SR AI 降噪 | 可用（需 PSRAM） | ✓（硬件更强但用不上） |

### 结论

HFP 蓝牙麦克风项目**必须使用 ESP32 原版**（WROVER 型号，带 PSRAM）。ESP32-S3 适合 BLE 外设或 USB 设备，但不适合任何依赖 Classic BT 的应用。

---

## 2. ESP32 Pin 选择建议

ESP32（原版 NodeMCU-32S / WROVER）的 I2S + 按钮引脚：

| 功能 | GPIO | 说明 |
|------|------|------|
| I2S SD (DATA) | 32 | INMP441 数据输入 |
| I2S WS (LRCLK) | 25 | I2S 字选择 |
| I2S SCK (BCLK) | 26 | I2S 位时钟 |
| Button 1 (PTT) | 13 | PTT 按住说话 |
| Button 2 | 12 | 快捷键 |
| Button 3 | 14 | 快捷键 |

这些引脚在 ESP32（原版）上安全可用，不与 PSRAM/Flash 冲突。

---

## 3. ESP-SR 集成指南

### 3.1 ESP-SR 是什么

ESP-SR 是乐鑫官方的语音 AI 算法库，包含：
- **NS (Noise Suppression)** — 神经网络降噪，区分人声和噪声
- **VAD (Voice Activity Detection)** — 语音活动检测，识别静音段
- **AGC (Auto Gain Control)** — 自动增益控制，均衡音量
- **AEC (Acoustic Echo Cancellation)** — 回声消除（本项目不需要）

ESP-SR 音频前端（AFE）将以上算法串联成管道，输入原始 PCM → 输出处理后 PCM。

### 3.2 依赖与版本

```
# idf_component.yml (src/ 目录下)
dependencies:
  espressif/esp-sr: "^2.0.0"
  espressif/esp-nn: "^1.1.0"
```

ESP-SR v2.x 支持 ESP-IDF v5.x，兼容 ESP32（Xtensa）。

### 3.3 AFE 配置（单麦、语音识别模式）

实际 API（基于 ESP-SR v2.x for ESP32）：

```c
#include "model_path.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"

// 1. 从分区加载模型
srmodel_list_t *models = esp_srmodel_init("srmodels");

// 2. 创建配置："M" = 单麦，AFE_TYPE_SR = 语音识别
afe_config_t *cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);

// 3. 自定义配置
cfg->aec_init = false;   // 纯麦克风，无需回声消除
cfg->se_init  = false;   // 单麦无需多麦波束成形
cfg->ns_init  = true;    // 噪声抑制 ← 核心
cfg->vad_init = true;    // 语音活动检测
cfg->agc_init = true;    // 自动增益控制
cfg->wakenet_init = false; // 不需要唤醒词
cfg->afe_perferred_core = 1; // AFE 任务运行在 Core 1

// 4. 检查配置一致性
cfg = afe_config_check(cfg);

// 5. 获取 AFE 接口句柄
const esp_afe_sr_iface_t *afe_iface = esp_afe_handle_from_config(cfg);
// ⚠️ esp_afe_handle_from_config 在 esp_afe_sr_models.h 中声明

// 6. 创建 AFE 数据实例
esp_afe_sr_data_t *afe_data = afe_iface->create_from_config(cfg);

// 7. 获取 feed/fetch 帧大小
int feed_samples  = afe_iface->get_feed_chunksize(afe_data);   // 通常 512
int fetch_samples = afe_iface->get_fetch_chunksize(afe_data);  // 通常 512
int channels      = afe_iface->get_channel_num(afe_data);       // 1

size_t feed_bytes  = feed_samples * channels * sizeof(int16_t);  // = 1024
size_t fetch_bytes = fetch_samples * sizeof(int16_t);            // = 1024
```

### 3.4 帧级处理循环（关键）

ESP-SR AFE 以**固定帧大小**工作（通常 512 采样 = 1024 字节 @ 16kHz mono）。I2S 读取通常不是帧对齐的（每次约 320 字节），需要**累积缓冲区**：

```c
static uint8_t accum_buf[2048];  // 需至少 2x feed_size
static size_t  accum_size = 0;

size_t process(const uint8_t *raw, size_t len, uint8_t **out) {
    *out = NULL;

    // 1. 累积不足一帧 → 存入缓冲区，等待
    if (accum_size + len < feed_bytes) {
        memcpy(accum_buf + accum_size, raw, len);
        accum_size += len;
        return 0;
    }

    // 2. 凑够一帧 → feed + fetch
    size_t need = feed_bytes - accum_size;
    memcpy(accum_buf + accum_size, raw, need);
    accum_size = 0;

    afe_iface->feed(afe_data, (int16_t *)accum_buf);
    afe_fetch_result_t *result = afe_iface->fetch(afe_data);

    // 3. 处理输出
    if (result && result->data && result->data_size > 0) {
        memcpy(out_buf, result->data, result->data_size);
        *out = out_buf;
        size_t out_len = result->data_size;

        // 3a. 将剩余原始数据移回累积缓冲区，递归处理
        size_t remaining = len - need;
        if (remaining > 0) {
            memcpy(accum_buf, raw + need, remaining);
            accum_size = remaining;
        }

        return out_len;
    } else {
        // VAD 静音帧 — 输出全零，保持 HFP 链路畅通
        memset(out_buf, 0, fetch_bytes);
        *out = out_buf;
        return fetch_bytes;
    }
}
```

### 3.5 重要设计细节

1. **VAD 静音帧也必须输出数据**（全零 PCM），不能返回 0。否则 HFP ring buffer 欠载，SCO 链路上发送坏帧，产生"哒哒哒"爆音。

2. **AFE 初始化在 PSRAM 之后**：`esp_sr_afe_init()` 必须在 PSRAM 初始化之后调用（ESP-IDF 在启动早期自动初始化 PSRAM），内部 buffer 用 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` 分配。

3. **分核运行**：AFE 配置 `afe_perferred_core = 1` 使其在 Core 1 运行，Core 0 留给 Bluedroid + HFP 回调。

4. **模型文件**：需要 `srmodels` 分区（SPIFFS，约 2MB）存储 NS/VAD 神经网络模型。烧录时需将模型文件写入该分区。

5. **`esp_afe_handle_from_config` 声明位置**：在 `esp_afe_sr_models.h`，不是 `esp_afe_sr_iface.h`。

6. **不要自己定义 AFE 类型**：`afe_config_t`、`afe_type_t`、`afe_mode_t`、`afe_fetch_result_t` 都由 ESP-SR 头文件定义。重复定义会导致编译冲突。

### 3.6 从 I2S 到 HFP 的完整数据流

```
I2S DMA read (~320B / 5ms, 不定长)
    │
    ▼
accumulator buffer (累积到 1024B = 1 AFE帧)
    │
    ▼
afe_iface->feed() → AFE pipeline [NS→VAD→AGC]
    │
    ▼
afe_iface->fetch() → 降噪后 1024B PCM
    │ (VAD静音帧 = 全零 1024B)
    ▼
Ring Buffer (9600 bytes)
    │
    ▼
bt_app_hf_outgoing_cb() → HCI → SCO → Windows
```

### 3.7 初始化序列

```c
void app_main() {
    // 1. NVS 初始化
    bt_nvs_init();

    // 2. 配置加载
    config_storage_init();

    // 3. I2S 麦克风 (16kHz, 16-bit, mono)
    audio_capture_init();

    // 4. ESP-SR AFE ← PSRAM 已可用
    esp_sr_afe_init();

    // 5. 蓝牙栈 (BTDM + HFP HF Client + BLE GATT)
    bt_stack_init();

    // 6. BLE GATT 服务
    ble_gatts_init();

    // 7. 按钮处理
    button_handler_init();

    // 主循环等待连接...
}
```

---

## 4. 硬件降噪建议（无需软件改动）

在 INMP441 模块上并联焊接电容（上一版验证有效）：

| 元件 | 位置 | 作用 |
|------|------|------|
| 100nF (0.1µF) 瓷片电容 | VDD ↔ GND | 滤高频数字噪声 |
| 10µF 电解电容 | VDD ↔ GND | 滤低频电源纹波 |
| 磁珠（可选） | VDD 线串联 | 抑制高频传导噪声 |

电容尽可能靠近 INMP441 模块。

---

## 5. PlatformIO + ESP-IDF sdkconfig 踩坑记录

### 5.1 sdkconfig.defaults 可能不生效

在 PlatformIO + ESP-IDF 5.x 组合下，`sdkconfig.defaults` 不一定会被自动应用（取决于 PlatformIO 版本和 ESP-IDF 版本的组合）。如果发现 sdkconfig 中没有预期配置：

**解决办法**：在 `platformio.ini` 中添加：

```ini
board_build.cmake_extra_args =
    -DSDKCONFIG_DEFAULTS=sdkconfig.defaults
```

这会显式告诉 CMake 使用你的 defaults 文件。

### 5.2 改动 sdkconfig.defaults 后要 clean

```bash
rm -rf .pio
pio run
```

否则 PlatformIO 会使用缓存的 sdkconfig，改变不生效。

### 5.3 ESP-SR 组件通过 IDF Component Manager 获取

`src/idf_component.yml` 中声明的依赖 `espressif/esp-sr` 会在首次编译时自动下载到 `managed_components/` 目录。该目录会在 `.gitignore` 中，不要提交。

---

## 6. 验证方法

### 编译
```bash
cd esp32_bt_mic && pio run
```

### 烧录
```bash
pio run -t upload --upload-port COMx
```

### 预期串口日志
```
I (1234) ESP_SR: AFE: feed=512 samples (1024B), fetch=512 samples (1024B), ch=1
I (1234) ESP_SR: AFE initialized successfully
I (1245) BT_INIT: BT stack initialized, name: ESP32_BT_MIC
I (1245) BT_HFP_HF: HF PROF STATE: Init Complete
I (1245) BLE_GATTS: BLE advertising started
I (3456) BT_HFP_HF: SLC connected, ready for PTT
I (3456) BTN_HANDLER: Button 1 pressed
I (3456) BT_HFP_HF: Audio State: CONNECTED_MSBC
```

### Windows 端验证
1. 蓝牙配对 ESP32_BT_MIC（PIN 0000 或 1234）
2. 打开 Windows 设置 → 系统 → 声音 → 录音标签页
3. 按住 Button 1（PTT），说话，观察电平表跳动
4. 打开 Windows 语音输入（Win+H），测试识别准确率

---

## 7. 参考资源

- ESP-SR 文档: https://docs.espressif.com/projects/esp-sr/
- ESP-SR GitHub: https://github.com/espressif/esp-sr
- ESP-IDF HFP HF 例程: `examples/bluetooth/bluedroid/classic_bt/hfp_hf/`
- ESP32 原版项目: `e:\AI_coding_test\ESP32_MIC_Claudecode\`
- ESP32-S3 Bluetooth Guide (确认不支持Classic BT): https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/api-guides/bluetooth.html

---

*文档生成于 2026-05-07，基于 ESP-IDF v5.5.0 + ESP-SR v2.x + PlatformIO 6.1.19*
