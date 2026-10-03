#!/usr/bin/env python3
"""Check source icon containers and, optionally, the package built on this OS."""
import argparse
import ctypes
from pathlib import Path
import plistlib
import struct
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--package', type=Path)
args = parser.parse_args()

def png(path, size):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    assert struct.unpack('>II', data[16:24]) == (size, size), path

for size in (16, 32, 48, 64, 128, 256, 512):
    png(args.source / f'resources/icons/app/{size}.png', size)
ico = (args.source / 'resources/ebalia.ico').read_bytes()
reserved, kind, count = struct.unpack('<HHH', ico[:6])
assert reserved == 0 and kind == 1 and count >= 1
ico_sizes = set()
for i in range(count):
    width, height, _, _, _, _, length, offset = struct.unpack_from('<BBBBHHII', ico, 6 + i * 16)
    assert offset + length <= len(ico)
    ico_sizes.add((width or 256, height or 256))
assert (256, 256) in ico_sizes
icns = (args.source / 'resources/ebalia.icns').read_bytes()
assert icns[:4] == b'icns' and struct.unpack('>I', icns[4:8])[0] == len(icns)
offset, entries = 8, set()
while offset < len(icns):
    tag, length = struct.unpack_from('>4sI', icns, offset)
    assert length >= 8 and offset + length <= len(icns)
    entries.add(tag)
    offset += length
assert {b'ic07', b'ic08', b'ic09', b'ic10'} <= entries

if args.package:
    package = args.package.resolve()
    if sys.platform == 'darwin':
        app = package / 'ebalia-launcher.app/Contents'
        with (app / 'Info.plist').open('rb') as f:
            info = plistlib.load(f)
        assert info['CFBundleIconFile'] == 'ebalia.icns'
        assert (app / 'Resources/ebalia.icns').read_bytes() == icns
    elif sys.platform == 'win32':
        executable = str(package / 'bin/ebalia-launcher.exe')
        extract = ctypes.windll.shell32.ExtractIconExW
        extract.argtypes = [ctypes.c_wchar_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint]
        assert extract(executable, -1, None, None, 0) > 0, 'EXE has no icon resource'
    else:
        desktop = (package / 'share/applications/ebalia-launcher.desktop').read_text()
        assert 'Icon=ebalia-launcher\n' in desktop and 'StartupWMClass=EBALIA Launcher' in desktop
        for size in (16, 32, 48, 64, 128, 256, 512):
            png(package / f'share/icons/hicolor/{size}x{size}/apps/ebalia-launcher.png', size)
print('Icon resources verified' + ('; native package verified' if args.package else ''))
