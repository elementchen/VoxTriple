# VoxTriple PCB 文件说明 (v1.5)

v1.5 改动：所有功能引脚集中到 ESP32 NodeMCU-32S **右侧排 (J2)**，接线更整洁。

## 推荐方式

这是一个连线板，推荐直接在嘉立创EDA中手动放置焊盘连线，详见 **[board_layout.md](board_layout.md)**。

## 文件列表

| 文件 | 用途 |
|------|------|
| `board_layout.md` | **主要 — PCB焊盘定位、布线表、操作步骤** |
| `VoxTriple.kicad.net` | KiCAD PcbNew 网表（标准库封装名） |

## 引脚分配总表 (v1.5)

全部位于 **右侧排 (J2)**：

| GPIO | 右排Pin | 功能 |
|------|---------|------|
| GPIO21 | J2-6 | I2S BCK |
| GPIO22 | J2-3 | I2S WS |
| GPIO17 | J2-11 | I2S DATA |
| GPIO18 | J2-9 | Button 1 |
| GPIO19 | J2-8 | Button 2 |
| GPIO16 | J2-12 | Button 3 |
| GPIO23 | J2-2 | LED 指示灯 |
| — | J2-1,7,19 | GND |
| — | J1-1 | 3V3（唯一左侧引线） |

## 注意事项

- **INMP441 VDD 必须接 3.3V**，不能接 5V（会烧毁麦克风）
- **GPIO12** 是启动引脚(MTDI)，不要连接任何东西
- **GPIO0** 是 BOOT 引脚，不能用作低电平按钮
- 按键为**低电平有效**，使用 ESP32 内部上拉
