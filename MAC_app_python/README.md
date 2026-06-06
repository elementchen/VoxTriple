# VoxTriple macOS Config App

macOS 版本的 VoxTriple 蓝牙键盘配置工具。

## 安装

```bash
cd MAC_app_python
pip install -r requirements.txt
```

## 运行

```bash
python vox_triple_mac.py
```

## 打包

```bash
pyinstaller VoxTriple_mac.spec
# 输出: dist/VoxTriple.app
```

## 权限

键盘捕获需要辅助功能权限：
系统设置 → 隐私与安全性 → 辅助功能 → 添加终端或 VoxTriple.app

## 共享代码

`ble_client.py` 和 `config_service.py` 与 Windows 版本共享（位于 `../windows_app_python/`），只有 `keyboard_io_mac.py` 和 `vox_triple_mac.py` 是 Mac 专属。
