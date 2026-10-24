# 项目工程规范与 Agent 协作指南 (AGENTS.md)

本项目为 **VoxTriple**（ESP32 蓝牙手持语音输入机械终端）全生命周期工程，集成了嵌入式固件、机械结构设计、硬件 PCB、跨平台上位机以及线上产品独立站。

---

## 1. 项目核心结构规范

本项目遵从标准化三核心目录体系规范（兼顾已有嵌入式编译构建与工程资产）：

```text
├── Docs/ (docs/)            # [文档中心] 所有需求、架构设计、BLE协议、开发笔记与上下文备忘录
├── Design/ (PCB/, 3d_print/) # [设计工程] 结构设计原始工程(3D外壳)、PCB原理图与硬件物料
├── Main/ (esp32_bt_mic/,    # [核心开发] ESP32固件源码、Windows/macOS上位机
│          windows_app_python/, 
│          MAC_app_python/)
├── Website/                 # [线上独立站] VoxTriple 电商长图页与官方品牌站(零构建前端)
└── AGENTS.md                # [本规范文件] AI 协同与边界准则
```

---

## 2. 多会话防污染守卫机制 (Scope Guard)

为防止不同业务线（嵌入式固件 vs 独立站前端）互相干扰与上下文污染，所有 Agent 会话必须严格执行分工与目录守卫：

### 2.1 硬件与固件开发线（主线）
- **操作范畴**：`esp32_bt_mic/`、`windows_app_python/`、`MAC_app_python/`、`PCB/`、`3d_print/`、`docs/`
- **严禁越界**：**严禁修改 `Website/` 下的任何代码与文件**。

### 2.2 独立站与电商线（站点线）
- **操作范畴**：**严格仅限 `Website/` 目录**。
- **严禁越界**：**严禁修改除 `Website/` 以外的任何固件、脚本、PCB 或 3D 工程文件**。
- **物料引用**：只读方式引用 `3d_print/` 或已有物料至 `Website/Medias/`，不可反向写入。

---

## 3. 上下文与记忆继承指南

- 当开启新会话或跨端切换（Antigravity IDE ↔ Antigravity 2.0 APP）时：
  - 可直接引用项目记忆备忘录：`@docs/PROJECT_CONTEXT_MEMORY.md`；
  - 或引用硬件主线核心会话：`[Debugging NimBLE Connection Issues](conversation://54b88e82-9e66-4bab-8726-e7f57cfd6771)`；
  - 或引用历史独立站规划会话：`[Website Planning](conversation://c1b7a0f7-97db-46af-9516-be10a80e1ac6)`。

---

## 4. Git 版本控制与提交闭环规范

- **提交时机**：每次与用户的对话结束前，凡涉及文件新增、代码修改或配置调整，必须自动执行本地 Git commit 规范提交，确保版本历史完整。
- **提交格式**：严格使用 Conventional Commits 规范，并在说明中以模块标识隔离：
  - 固件/硬件改动：`feat(firmware): ...` 或 `fix(hardware): ...`
  - 独立站改动：`feat(website): ...` 或 `style(website): ...`
  - 文档与规范：`docs: ...`
