# -*- mode: python ; coding: utf-8 -*-
# 用法：pyinstaller shasha.spec

from PyInstaller.utils.hooks import collect_submodules

block_cipher = None

a = Analysis(
    ['main.py'],
    pathex=['.'],
    binaries=[],
    datas=[
        ('data',  'data'),    # 图片、toml、ini 等资源
        ('games', 'games'),   # 游戏子包
    ],
    hiddenimports=[
        *collect_submodules('games'),
        'tomllib',            # Python 3.11+
        'tomli',              # fallback，没装也无妨
        'psutil',             # CPU 监控，可选
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='shasha',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,                        # 不显示控制台窗口
    icon='data\\tray.ico',                # 需要准备 .ico 格式图标
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='shasha',                        # 输出目录 dist/shasha/
)