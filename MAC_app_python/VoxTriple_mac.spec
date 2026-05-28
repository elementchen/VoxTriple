# -*- mode: python ; coding: utf-8 -*-

import os, sys
# Add shared modules path so PyInstaller can find them
_shared = os.path.join(os.path.dirname(__file__), '..', 'windows_app_python')
if os.path.isdir(_shared):
    sys.path.insert(0, os.path.abspath(_shared))

a = Analysis(
    ['vox_triple_mac.py'],
    pathex=[_shared],
    binaries=[],
    datas=[],
    hiddenimports=['bleak.backends.corebluetooth', 'pynput.keyboard._darwin'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=['tkinter.test'],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='VoxTriple',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='VoxTriple',
)

app = BUNDLE(
    coll,
    name='VoxTriple.app',
    icon=None,
    bundle_identifier='com.voxtriple.config',
    version='1.7.0',
    info_plist={
        'NSHighResolutionCapable': True,
        'NSBluetoothAlwaysUsageDescription': 'VoxTriple uses Bluetooth to configure your ESP32 BT Microphone.',
        'LSBackgroundOnly': False,
    },
)
