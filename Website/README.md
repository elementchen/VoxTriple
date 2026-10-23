# VoxTriple 线上产品站点体系 (Website)

本目录为 **VoxTriple**（ESP32 经典蓝牙 HID 键盘与 HFP 免提麦克风双模便携终端）的全部线上展示站点工程。

## 目录组织结构

```text
Website/
├── Medias/       # [共享媒体资源] 包含 3D 打印渲染图、AI 咖啡厅/办公室场景写真图、演示视频
│
├── Ecommerce/    # [电商产品详情页] 专注产品营销转化、多视角展示、版本选配联动与加购体验
│   ├── index.html
│   ├── assets/
│   │   ├── css/style.css
│   │   └── js/main.js
│   └── README.md
│
├── Official/     # [品牌独立产品官网] 专注苹果级产品美学、声学解构、在线按键互动模拟器、OTA 与固件下载
│   ├── index.html
│   ├── assets/
│   │   ├── css/style.css
│   │   └── js/main.js
│   └── README.md
│
└── README.md     # [本说明文件] 整体架构与使用指引
```

## 两大站点设计定位对比

| 维度 | 电商产品页 (`Ecommerce/`) | 独立产品官网 (`Official/`) |
| :--- | :--- | :--- |
| **主要目标** | 购买转化、消除选购顾虑、展示开箱清单与配件 | 品牌形象立标、深度技术微架构解构、极客互动体验 |
| **主打功能** | 颜色切换、DIY套件 vs 官方整机选配器、吸顶购买栏、FAQ | 按键模拟器（联动上位机终端）、20秒 OTA 模拟演示、固件下载 |
| **视觉风格** | 高对比商品展示、场景痛点卡片、购买保障背书 | 苹果级极光暗黑风、大字排版、沉浸式产品写真与声学动效 |
| **入口文件** | [Ecommerce/index.html](Ecommerce/index.html) | [Official/index.html](Official/index.html) |

## 启动与预览方式
由于采用标准现代零构建体系（HTML5 + Tailwind CSS + Lucide Icons + 原生 ES6+），无需安装 npm 依赖：
1. **直接双击**：在任意现代浏览器中双击 `index.html` 即可完整浏览全部交互；
2. **轻量静态服务器**：在 `Website/` 目录下执行 `python -m http.server 8080`，即可通过浏览器访问 `http://localhost:8080/Ecommerce/` 或 `http://localhost:8080/Official/`。
