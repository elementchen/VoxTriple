# VoxTriple PCB 文件说明

## 文件列表

| 文件 | 用途 | 导入方式 |
|------|------|----------|
| `VoxTriple.enet` | 嘉立创EDA(标准版/专业版) | 文件 → 导入 → Altium/Protel 网表 |
| `VoxTriple.kicad.net` | KiCAD 7.x / 8.x | PcbNew → 文件 → 导入网表 → 选择此文件 |

## 导入步骤

### 嘉立创EDA (EasyEDA / LCEDA)

1. 打开嘉立创EDA（标准版或专业版）
2. 新建一个PCB文件
3. 菜单：**文件 → 导入 → Altium/Protel**
4. 选择 `VoxTriple.enet`
5. 在弹出的窗口中确认封装匹配（首次导入需要手动选择封装）

**注意**：导入网表前，需要先在原理图中放置好元件符号，或者导入后手动在PCB中放置封装。网表只定义了连接关系，不包含元件库。

### KiCAD

1. 打开 PcbNew（PCB编辑器）
2. 菜单：**文件 → 导入 → Netlist...**
3. 选择 `VoxTriple.kicad.net`
4. 点击 "更新PCB" / "Update PCB"

**注意**：建议先复制为 `VoxTriple.net` 再导入。

## 引脚映射表

### ESP32 NodeMCU-32S (MOD1) — 38脚

| 物理脚 | 网络名 | GPIO | 说明 |
|--------|--------|------|------|
| 1 | 3V3 | — | 3.3V 输出，给 INMP441 供电 |
| 8 | I2S_WS | GPIO33 | I2S 字选 (WS/LRCLK) |
| 9 | I2S_BCK | GPIO25 | I2S 位时钟 (SCK/BCLK) |
| 10 | LED_NET | GPIO26 | 指示灯（经220Ω电阻到LED） |
| 12 | BTN2 | GPIO14 | 按键2（低电平有效） |
| 14 | GND | — | 地 |
| 20 | GND | — | 地 |
| 26 | GND | — | 地 |
| 28 | BTN1 | GPIO18 | 按键1（低电平有效） |
| 30 | I2S_DATA | GPIO17 | I2S 数据 (SD/DIN) |
| 31 | BTN3 | GPIO16 | 按键3（低电平有效） |
| 38 | GND | — | 地 |

### INMP441 (MOD2) — 6脚

| 脚位 | 网络名 | 说明 |
|------|--------|------|
| 1 | 3V3 | VDD（**必须3.3V，不能接5V！**） |
| 2 | GND | 地 |
| 3 | GND | L/R 声道选择（接GND=左声道） |
| 4 | I2S_DATA | SD (Serial Data) |
| 5 | I2S_WS | WS (Word Select / LRCLK) |
| 6 | I2S_BCK | SCK (Bit Clock / BCLK) |

### 按钮 SW1-SW3

| 按键 | 网络名 | GPIO | 连接 |
|------|--------|------|------|
| SW1 | BTN1 | GPIO18 | 脚1→GPIO18, 脚2→GND |
| SW2 | BTN2 | GPIO14 | 脚1→GPIO14, 脚2→GND |
| SW3 | BTN3 | GPIO16 | 脚1→GPIO16, 脚2→GND |

### LED 指示灯

```
GPIO26 → R1(220Ω) 脚1 → R1(220Ω) 脚2 → LED1 阳极(A) → LED1 阴极(K) → GND
```

## 物料清单 (BOM)

| 数量 | 编号 | 元件 | 封装建议 |
|------|------|------|----------|
| 1 | MOD1 | ESP32-WROOM-32 NodeMCU 38P | 2x19P 2.54mm 排母 |
| 1 | MOD2 | INMP441 I2S MEMS Mic | 1x6P 2.54mm 排母 |
| 3 | SW1-3 | Tactile Switch 6x6mm | 6x6mm 直插/贴片 |
| 1 | LED1 | Red LED 3mm | 3mm 直插 |
| 1 | R1 | 220Ω | 0805 贴片 或 1/4W 直插 |

## 注意事项

1. **INMP441 VDD 必须接 3.3V**，绝不能接 5V，否则会烧毁麦克风
2. **GPIO12** 是启动引脚(MTDI)，不要在启动时拉低，避免用于低电平按钮
3. 按钮为**低电平有效**（按下时 GPIO 接地），使用 ESP32 内部上拉
4. 所有模块**共地**
