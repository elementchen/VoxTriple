# VoxTriple PCB 文件说明

## 推荐使用方式

这是一个**连线板（Breakout Board）**，元件很少（2个模块 + 3个按键 + LED + 电阻），
**推荐直接在嘉立创EDA或KiCAD中手动放置焊盘然后连线**，不需要导入网表。

详细操作指南见 **[board_layout.md](board_layout.md)**。

## 文件列表

| 文件 | 用途 |
|------|------|
| `board_layout.md` | **主要文件 — PCB焊盘定位、布线表、操作步骤** |
| `VoxTriple.kicad.net` | KiCAD PcbNew 网表（用标准库封装名，可直接导入） |

## KiCAD 网表导入步骤

1. 打开 KiCAD → PcbNew（PCB编辑器）
2. 文件 → 导入 → Netlist → 选择 `VoxTriple.kicad.net`
3. KiCAD 会自动匹配标准库中的封装：
   - 两个 `PinHeader_1x19_P2.54mm_Vertical`（ESP32 两排排母）
   - 一个 `PinHeader_1x06_P2.54mm_Vertical`（INMP441）
   - 三个 `SW_PUSH_6mm`（按键）
   - 一个 `LED_D3.0mm`（LED）
   - 一个 `R_0805_2012Metric`（电阻）
4. 点击 "Update PCB" — 封装会自动加载到 PCB 画布
5. 手动摆放元件位置，按布线表连线

## 嘉立创EDA手动创建步骤

嘉立创EDA不支持独立导入网表（其"导入→Altium"只认完整原理图），
所以请在嘉立创EDA中手动创建，参考 `board_layout.md` 中的：

1. 焊盘孔径和外径尺寸
2. 两排19脚 ESP32 排母定位（间距2.54mm，排距22.86mm）
3. 单排6脚 INMP441 排母定位（间距2.54mm）
4. 3个按键 + LED + 电阻的焊盘
5. 布线连接表（10条网络线）

对于这个简单的板子，手动创建大约需要 15-20 分钟。

## 引脚分配（编译值，来自 sdkconfig）

| GPIO | 功能 | 连接至 |
|------|------|--------|
| GPIO25 | I2S BCK (位时钟) | INMP441 SCK |
| GPIO33 | I2S WS (字选) | INMP441 WS |
| GPIO17 | I2S DATA (数据) | INMP441 SD |
| GPIO18 | Button 1 | SW1 → GND |
| GPIO14 | Button 2 | SW2 → GND |
| GPIO16 | Button 3 | SW3 → GND |
| GPIO26 | 指示灯 | R1(220Ω) → LED 阳极 → 阴极 → GND |

## 注意事项

- **INMP441 VDD 必须接 3.3V**，不能接 5V（会烧毁麦克风）
- **GPIO12** 是启动引脚(MTDI)，不要连接任何东西
- ESP32 的 GPIO34-39 是输入专用引脚，不能用于输出
- 按键为**低电平有效**（按下短路到 GND），使用 ESP32 内部上拉
