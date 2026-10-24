# 项目工程规范与 Agent 协作指南 (AGENTS.md)

本项目为 **VoxTriple**（ESP32 蓝牙手持语音输入机械终端）全生命周期工程。

---

## 1. 核心开发与会话架构

根据用户最新决策，项目采用**主线单会话全栈协作模式**，由本核心会话统筹 ESP32 固件与上位机桌面端开发：
- **ESP32 嵌入式固件**：位于 `esp32_bt_mic/`
- **Windows 上位机客户端**：位于 `windows_app_python/`
- **设计与架构文档**：位于 `docs/`

---

## 2. GitHub 提交范围边界与安全隔离机制 (Commit Scope Guard)

为保护非开源资产、敏感工程数据并保持代码仓库的纯粹整洁，本项目严格划分 **AI 自动提交范畴** 与 **本地私有/排除范畴**：

### 2.1 AI 允许维护与自动提交范畴 (Committable Scope)
AI 仅对以下核心目录及其必要根级配置文件拥有修改、编译与本地 Git commit 权限：
- `esp32_bt_mic/`（ESP32 固件源码、Kconfig、分区表及编译规则）
- `windows_app_python/`（Windows KeyMapper 上位机源码、Web 前端与打包配置）
- `docs/`（需求说明、硬件架构、通信协议、项目备忘录）
- 根级工程与规范文件（`AGENTS.md`、`.gitignore`、`VERSION_LOG.md` 等）

### 2.2 严禁提交范畴 (Strictly Excluded Scope)
以下目录属于私有资产、非开源物料或独立管理内容，**已全部纳入 `.gitignore`，严禁 AI 进行 `git add` 或提交到 GitHub**：
- `_Archive/`（历史归档代码与旧工程）
- `3d_print/`（3D 建模原始文件、切片工程、模型物料）
- `MAC_app_python/`（macOS 端上位机资产）
- `PCB/`（硬件原理图与立创/EDA 原始工程文件）
- `Serial_Log/`（本地调试日志与诊断抓包）
- `Website/`（线上独立站与电商全量前端物料）

> [!NOTE] 用户手动管理原则
> 上述排除目录完全保留在用户本地硬盘上，由用户根据自身开源规划与需求，后续在本地通过 `git add -f` 手动选择性上传至 GitHub，AI 绝不越权干预或提交。

---

## 3. 上下文与记忆继承指南

- 当开启新会话或跨端切换（Antigravity IDE ↔ Antigravity 2.0 APP）时：
  - 可直接引用项目记忆备忘录：`@docs/PROJECT_CONTEXT_MEMORY.md`；
  - 或引用主线核心会话：`[Debugging NimBLE Connection Issues](conversation://54b88e82-9e66-4bab-8726-e7f57cfd6771)`。

---

## 4. Git 版本控制与提交闭环规范

- **提交时机**：每次与用户的对话结束前，凡涉及文件新增、代码修改或配置调整，必须自动执行本地 Git commit 规范提交，确保版本历史完整。
- **提交格式**：严格使用 Conventional Commits 规范，并在说明中以模块标识隔离：
  - 固件改动：`feat(firmware): ...` 或 `fix(firmware): ...`
  - 上位机客户端改动：`feat(client): ...` 或 `fix(client): ...`
  - 文档与规范：`docs: ...`
  - 工程与忽略规则：`chore: ...`
