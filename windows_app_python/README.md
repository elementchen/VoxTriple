# VoxTriple — Python Edition

Python + tkinter rewrite of the Windows config app. Same functionality as the C# WPF version.

## Requirements

- Python 3.10+
- Windows 10/11

## Install

```bash
pip install -r requirements.txt
```

## Run

```bash
python vox_triple.py
```

## Build standalone EXE (optional)

```bash
pip install pyinstaller
pyinstaller --onefile --windowed --name VoxTriple vox_triple.py
```

## Modules

| File | Purpose |
|------|---------|
| `vox_triple.py` | Main tkinter GUI + app logic |
| `ble_client.py` | BLE GATT client (bleak) |
| `keyboard_io.py` | Win32 keyboard hook + keybd_event simulation |
| `config_service.py` | JSON config persistence |
