# VoxTriple 官方独立产品网站 (Official Product & Developer Website)

本目录为 VoxTriple 品牌官方产品展示与固件开发者中心的前端工程。

## 目录结构
```text
Official/
├── index.html           # 官方产品站主入口
├── assets/
│   ├── css/style.css    # 苹果级极光暗黑风格、毛玻璃拟态、按键模拟器物理触感微动效
│   └── js/main.js       # 杀手级互动装置：4键按键映射与上位机串口控制台实时捕获、20秒闪电 OTA 模拟器、音频波形动画
└── README.md            # 本文件
```

## 核心设计与特性
1. **苹果级 Cinematic Hero 与叙事**：
   - 沉浸式大字排版（`VoxTriple. 灵感，一触即发。`）；
   - 产品 3D 渲染图（`../Medias/Final03.png`）居中特写与浮动硬件参数标签；
   - 讲述重塑手持交互的设计哲学（从办公室到咖啡厅随行）。
2. **杀手级互动装置 (Live Interactive Lab)**：
   - 在网页上提供一个高保真 VoxTriple 掌上设备面板；
   - 支持鼠标点击或敲击键盘物理数字键 `[1]` `[2]` `[3]` `[4]`，模拟实体轻触按键下沉与板载蓝光 LED 指示；
   - 联动右侧“上位机实时控制台”，动态输出真实串口日志（HID 键值、mSBC 宽带音频流启动、状态变更）。
3. **声学与 DSP 算法解构**：
   - INMP441 MEMS + I2S 通路；
   - 硬件级 DSP 高通滤波 + 滑动平均降噪动效；
   - 16kHz mSBC 宽带语音通道可视化。
4. **闪电 OTA 模拟器与固件资源中心**：
   - 20 秒极速有线 OTA 实时模拟演示进度条；
   - Windows 配置工具、一键合并固件 bin 与 3D 打印模型源文件下载矩阵。

## 本地预览
直接在浏览器中打开 `index.html`，或通过本地任意静态服务器（如 `python -m http.server 8080`）启动访问。
