# VoxTriple 线上产品站点体系 (Website)

本目录为 **VoxTriple**（专为台式机与 AI 时代打造的掌上语音输入机械终端）的全部线上展示站点工程。

## 目录组织结构

```text
Website/
├── Medias/       # [共享媒体资源] 包含 3D 打印渲染图、AI 场景写真图、CAD尺寸工程图、演示视频
│
├── Ecommerce/    # [电商详情长图页] 天猫/淘宝旗舰店纯白风格，专注击穿痛点、场景化带入与快速转化
│   ├── index.html
│   ├── assets/
│   │   ├── css/style.css
│   │   └── js/main.js
│   └── README.md
│
├── Official/     # [品牌官方网站] 深度对标 Codex 哲学、声学与算法、在线互动模拟器、OTA与固件中心
│   ├── index.html
│   ├── assets/
│   │   ├── css/style.css
│   │   └── js/main.js
│   └── README.md
│
└── README.md     # [本说明文件] 架构与快速上手指引
```

## 核心产品定位与参数

- **核心定位**：专为台式电脑（无麦克风）与 AI 对话（海量 Prompt 需求）打造的随身手持语音输入机械终端。
- **ASR 生产力**：配合 Windows / macOS 原生语音输入法（Win+H / 听写），达到 200~300 字/分钟的高速口述出字，提升效率 300%。
- **硬件三围**：`18.5 × 29.8 × 102.0 mm`（比口红更纤巧轻便）。
- **按键手感**：4 × 客制化机械键盘轴体，支持 PTT（按住说话、松开静音）及盲操回车发送。
- **电池续航**：轻薄聚合物锂电池，**待机长达 7 天，持续使用支撑 1 整天**。
- **连接方式**：经典蓝牙 HID + HFP 免驱动双模复合设备。

## 启动与预览方式
由于采用标准现代化零构建体系（HTML5 + Tailwind CSS + Lucide Icons + 原生 ES6+），无需安装 npm 依赖：
1. **直接双击**：在任意现代浏览器中双击 `index.html` 即可完整浏览全部交互；
2. **轻量静态服务器**：在 `Website/` 目录下执行 `python -m http.server 8080`，即可通过浏览器访问 `http://localhost:8080/Ecommerce/` 或 `http://localhost:8080/Official/`。
