# VoxTriple 电商产品详情页 (E-Commerce Product Landing Page)

本目录为 VoxTriple 针对电商（独立站/众筹/产品发售详情页）设计的前端工程。

## 目录结构
```text
Ecommerce/
├── index.html           # 电商落地页主入口
├── assets/
│   ├── css/style.css    # 苹果级极简电商样式、悬浮购买条、玻璃拟态面板
│   └── js/main.js       # 多媒体画廊、颜色选配、DIY/成品套餐联动计算、购买模态框
└── README.md            # 本文件
```

## 核心设计与特性
1. **多媒体交互相册**：无缝调用上级 `../Medias/` 中的 3D 渲染图、办公场景实机图以及动态演示视频，支持切换缩放。
2. **产品选配器 (Configurator)**：
   - 颜色切换：深空曜黑 (Stealth Black) / 工业透灰 (Cyber Gray) / 极简冷白 (Frost White)。
   - 版本选配：创客 DIY 套件 (¥199) vs 官方旗舰成品 (¥299)，实时计算总价。
3. **转化触点**：
   - 底部吸顶快捷购买栏 (Sticky Buy Bar)：随滚动动态唤起，确保转化入口常驻。
   - 场景化痛点映射：咖啡厅轻声低语录入、极客工位一键触发 AI 与代码、会议演讲 PTT 防喷防误触。
   - 规格表与 FAQ 手风琴：解答免驱机制、OTA 固件升级与电池续航疑问。

## 本地预览
直接在浏览器中打开 `index.html`，或通过本地任意静态服务器（如 `python -m http.server 8080`）启动访问。
