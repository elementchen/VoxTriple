# VoxTriple 开发板硬件规格与历史兼容规划书
> **文档版本**：v1.1  
> **适用硬件**：  
> 1. ESP32 Lite V1.0.0（LOLIN32 Lite 兼容款 / **官方唯一标准硬件**）  
> 2. WEMOS ESP32 Board with 18650 Battery Holder（**已停产弃用归档**）  
> **状态**：历史方案备忘与技术档案（18650 款因体积厚重已被正式弃用，主线全面采用 ESP32 Lite V1.0.0 轻薄硬件标准）

---

## 1. 背景与目标

为了满足产品后续系列化、轻量化与结构微型化的需求，项目计划引入 **ESP32 Lite V1.0.0** 开发板。由于两款开发板在尺寸、引脚分布及板载硬件外设上存在物理差异，本项目提出一套**“同一套固件自适应双板、上位机智能识别并激活对应引脚配置”**的完整软硬件解决方案。

### 核心设计原则
1. **按键引脚零破坏性兼容**：两款开发板的 4 个物理按键使用**完全相同**的 GPIO 引脚，避免按键逻辑和用户使用习惯的分化。
2. **麦克风与 LED 引脚精准适配**：针对 ESP32 Lite V1 缺少 GPIO 21 以及板载 LED 变动的情况，提供最优接线，并在底层通过板级抽象层（Board Profile）动态加载。
3. **上位机可视化识别**：客户端（Windows / macOS）连接设备后能自动识别开发板型号，展示对应板卡铭牌，并支持用户切换写入生效。
4. **单固件全生命周期维护**：云端 OTA 与本地烧录无需维护两套 bin，固件根据内部 NVS 存储的硬件型号自适应初始化硬件。

---

## 2. 硬件引脚特性与差异深度对比

基于对原板原理图及仓库内 `docs/lolin32.pdf`（ESP32 Lite / LOLIN32_Lite V1.0.0 原厂原理图）的交叉比对，两款板卡的关键差异如下：

| 关键要素 | WEMOS 18650 电池版（原板） | ESP32 Lite V1.0.0（新板） | 差异影响与设计对策 |
| :--- | :--- | :--- | :--- |
| **板载 LED** | **GPIO 16**（低电平点亮） | **GPIO 22**（低电平点亮） | 新板物理固化在 GPIO 22，固件需根据板型切换 LED 驱动引脚。 |
| **GPIO 21 引出情况** | **正常引出**（用作 I2S BCK） | **未引出！**（物理排针无 Pin 21） | **最大物理差异**。新板必须改用其他空闲数字 GPIO 作为 I2S 时钟。 |
| **GPIO 22 引出情况** | 正常引出（原用作 I2S WS） | 引出，但**已被板载 LED 占用** | 新板不能再用 GPIO 22 接麦克风 WS，否则 16kHz 时钟会干扰 LED。 |
| **GPIO 17 引出情况** | 正常引出（用作 I2S DATA） | 正常引出（右侧） | 两款板均有且均为空闲引脚。 |
| **GPIO 4 (RTC IO 10)** | 正常引出（Button 1，支持唤醒） | 正常引出（右侧） | 两款板均支持 RTC Deep Sleep 唤醒，引脚完全对齐。 |
| **GPIO 18 / 19 / 23** | 正常引出（Button 2 / 3 / 4） | 正常引出（右侧） | 两款板均支持，引脚完全对齐。 |
| **电池接口** | 板载 18650 电池座 | 板载 2.0mm JST 锂电插座 + TP4054 | 供电方式变轻巧，接口不同，核心供电逻辑一致。 |
| **输入专用引脚** | GPIO 34, 35, 36, 39 | GPIO 34, 35 | 仅限输入，无内部上下拉，不建议用作普通按键或输出。 |
| **启动引脚 (Strapping)**| GPIO 0, 2, 12, 15 | GPIO 0, 2, 12, 15 | GPIO 12 严禁接强上拉（否则 Flash 供电错误导致变砖），避开使用。 |

---

## 3. ESP32 Lite V1.0.0 推荐接线方案

结合引脚物理可用性、抗干扰要求以及不强制同一侧引脚的原则，针对 **ESP32 Lite V1.0.0** 制定如下推荐方案：

### 3.1 核心接线定义表（ESP32 Lite V1.0.0）

| 模块 / 外设 | 外设引脚 | ESP32 Lite V1 推荐引脚 | 排针方位 | 详细说明 |
| :--- | :--- | :--- | :--- | :--- |
| **INMP441** | **VDD** | **3V3** | 左侧排 Pin 2 | 必须接 3.3V，严禁接 5V |
| | **GND** | **GND** | 左侧排 Pin 4 | 共地 |
| | **L/R** | **GND** | 左侧排 Pin 4 | 接地选择左声道 |
| | **SD** (DATA) | **GPIO 25** | 左侧排 Pin 9 | I2S 串行数据输入 |
| | **WS** (LRCLK) | **GPIO 26** | 左侧排 Pin 10 | I2S 声道字选择（时钟） |
| | **SCK** (BCLK) | **GPIO 27** | 左侧排 Pin 11 | I2S 位时钟 |
| **按键 1** | 引脚一端 | **GPIO 4** | 右侧排 Pin 9 | 语音 PTT 键 / Deep Sleep 唤醒键（低电平有效） |
| | 引脚另一端 | **GND** | 右侧排 Pin 1 | 按键按下时接地，内部弱上拉 |
| **按键 2** | 引脚一端 | **GPIO 18** | 右侧排 Pin 5 | 快捷键 2（低电平有效） |
| | 引脚另一端 | **GND** | 右侧排 Pin 1 | 按键按下时接地，内部弱上拉 |
| **按键 3** | 引脚一端 | **GPIO 19** | 右侧排 Pin 3 | 快捷键 3（低电平有效） |
| | 引脚另一端 | **GND** | 右侧排 Pin 1 | 按键按下时接地，内部弱上拉 |
| **按键 4** | 引脚一端 | **GPIO 23** | 右侧排 Pin 4 | 快捷键 4（低电平有效） |
| | 引脚另一端 | **GND** | 右侧排 Pin 1 | 按键按下时接地，内部弱上拉 |
| **板载 LED** | 贴片 LED | **GPIO 22** | 板载内部物理固化 | **无需外接任何引线**，固件自动控制（低电平亮，高电平灭） |

---

### 3.2 接线方案设计优势分析

1. **按键 100% 保持统一**：
   * **Button 1 ~ 4 依然完全使用 GPIO 4, 18, 19, 23**。
   * 好处：两款板的按键硬件定义一模一样，按键扫描、消抖、Deep Sleep RTC 唤醒逻辑完全共享，代码零分叉。
2. **INMP441 左侧集中式布线（极为优雅）**：
   * 在 ESP32 Lite V1.0.0 的左侧排针上，`3V3`、`GND`、`GPIO 25`、`GPIO 26`、`GPIO 27` 是**完全连续相邻**的排针！
   * 麦克风的 5 根杜邦线可以做成一个一体化的 5-Pin 排插直接插在左侧，走线极短，完全不需要横跨板子飞线，极大地降低了高频 I2S 信号的高频串扰与杂音。
   * GPIO 25, 26, 27 是标准通用 GPIO，且不属于 Boot Strapping Pin，对系统启动无任何不良影响。
3. **LED 彻底解耦**：
   * 原板：LED 走 GPIO 16；
   * 新板：LED 走 GPIO 22。
   * 固件层只定义一个逻辑 LED 接口，硬件层根据型号动态映射输出。

---

## 4. 固件架构设计（单固件双板兼容）

为了避免产生 `firmware_wemos.bin` 与 `firmware_lite.bin` 导致维护分叉与 OTA 混乱，采用**“运行时板级参数抽象”**机制。

### 4.1 数据结构定义（Board Profile）
在固件中定义硬件型号枚举与引脚配置结构体：

```c
typedef enum {
    BOARD_MODEL_WEMOS_18650 = 0, // 经典 18650 版
    BOARD_MODEL_ESP32_LITE_V1 = 1, // ESP32 Lite V1.0.0 版
} board_model_t;

typedef struct {
    board_model_t model;
    const char *model_name;
    gpio_num_t led_gpio;
    gpio_num_t i2s_bck_gpio;
    gpio_num_t i2s_ws_gpio;
    gpio_num_t i2s_data_gpio;
    gpio_num_t btn_gpios[4]; // 均为 4, 18, 19, 23
} board_profile_t;
```

预设配置表：
```c
static const board_profile_t s_board_profiles[] = {
    [BOARD_MODEL_WEMOS_18650] = {
        .model = BOARD_MODEL_WEMOS_18650,
        .model_name = "WEMOS 18650",
        .led_gpio = GPIO_NUM_16,
        .i2s_bck_gpio = GPIO_NUM_21,
        .i2s_ws_gpio = GPIO_NUM_22,
        .i2s_data_gpio = GPIO_NUM_17,
        .btn_gpios = { GPIO_NUM_4, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23 }
    },
    [BOARD_MODEL_ESP32_LITE_V1] = {
        .model = BOARD_MODEL_ESP32_LITE_V1,
        .model_name = "ESP32 Lite V1",
        .led_gpio = GPIO_NUM_22,
        .i2s_bck_gpio = GPIO_NUM_27,
        .i2s_ws_gpio = GPIO_NUM_26,
        .i2s_data_gpio = GPIO_NUM_25,
        .btn_gpios = { GPIO_NUM_4, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23 }
    }
};
```

### 4.2 NVS 持久化存储
* 在 NVS 的 `bt_config` 命名空间下新增 `hw_model` 键（`uint8_t`，默认为 `0`）。
* 设备上电初始化时：
  1. `config_storage_load_board_model(&model)` 读取配置；
  2. 若 NVS 为空，默认加载 `BOARD_MODEL_WEMOS_18650`（保证所有旧设备 OTA 后平滑过渡无感）；
  3. 系统将对应的 `led_gpio` 注册给 `button_handler`，将 `i2s_*_gpio` 注册给 `audio_capture`；
  4. 当收到客户端切换板型指令时，更新 NVS 并重启生效。

---

## 5. SPP 串口协议扩充设计

在现有的 SPP 二进制命令集（`uart_console.c`）基础上扩充 2 个指令：

### 5.1 命令定义

| 命令字 (CMD) | 名称 | 方向 | 数据长度 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| **0x0C** | `CMD_GET_BOARD_MODEL` | PC $\rightarrow$ 板 | 0 Byte | 请求当前板卡型号 |
| **0x0C** (ACK)| `CMD_GET_BOARD_MODEL_ACK` | 板 $\rightarrow$ PC | 2 Bytes | 返回：`[Status(0x00), ModelID(0/1)]` |
| **0x0D** | `CMD_SET_BOARD_MODEL` | PC $\rightarrow$ 板 | 1 Byte | 设置板卡型号：`[ModelID(0/1)]` |
| **0x0D** (ACK)| `CMD_SET_BOARD_MODEL_ACK` | 板 $\rightarrow$ PC | 1 Byte | 返回：`[Status(0x00)]`，设备随后自动重启 |

---

## 6. 上位机客户端（windows_app_python）交互与 UI 升级设计

现有的配置工具已升级为现代 WebView 架构。在此基础上进行如下无缝增强：

1. **自动读取并展示板卡铭牌**：
   * 客户端在点击 `CONNECT` 建立串口握手后，自动通过 `CMD_GET_BOARD_MODEL` 获取硬件版本。
   * 在状态栏与固件信息旁增加 **“硬件型号 (Hardware Model)”** 徽章：
     * 显示：`🟢 Wemos 18650 电池版` 或 `🟣 ESP32 Lite V1.0.0`。
2. **硬件型号切换与激活入口**：
   * 在配置界面底部或固件区增加“切换硬件型号”的下拉选择框。
   * 当用户需要将固件刷入新开发板，或者组装了 ESP32 Lite V1.0.0 时，可在此处选择 `ESP32 Lite V1.0.0`。
   * 点击 `Save & Switch / 保存并激活板型` 后，客户端下发 `CMD_SET_BOARD_MODEL`，开发板完成配置后自动重启，即刻激活新引脚映射。
3. **配置文件隔离机制（按板型保存配置）**：
   * 客户端本地保存按键预设时，以板型为维度进行预设关联（例如 `config_wemos_18650.json` 与 `config_esp32_lite.json`），用户在不同硬件间切换时，按键配置一键精准导入。

---

## 7. 开发路线与实施计划（Roadmap）

严格遵循“先论证、后开发、绝不破坏成熟功能”原则，分阶段推进：

- [x] **阶段一：硬件差异调研与引脚方案规划**（当前已完成，形成本文档）
- [ ] **阶段二：用户评审确认**
  * 确认接线方案（尤其是麦克风使用 GPIO 25, 26, 27 的便利性）；
  * 确认单固件自适应方案是否满足系列化生产需求。
- [ ] **阶段三：固件板级抽象层实现**
  * 编写 `board_profile.h` 与 `board_profile.c`；
  * 重构 `audio_capture.c` 与 `button_handler.c`，使用动态引脚变量取代宏定义；
  * `uart_console.c` 实现板型读写 SPP 指令。
- [ ] **阶段四：客户端识别与配置 UI 实现**
  * `spp_client.py` 封装 `read_board_model` 与 `write_board_model`；
  * `web/index.html` 与 `web/app.js` 增加开发板型号渲染和切换交互。
- [ ] **阶段五：双板实物联调与验证**
  * 测试 Wemos 18650 原有功能的回归（100% 兼容）；
  * 测试 ESP32 Lite V1.0.0 的麦克风录音音质、按键响应及 Deep Sleep 唤醒。
