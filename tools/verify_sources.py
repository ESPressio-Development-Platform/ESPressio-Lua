#!/usr/bin/env python3
"""Verify exact vendored Lua bytes and equivalence of standalone demo sources."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'vendor/SHA256SUMS.json').read_text())
vendor = root / 'vendor/lua'
actual = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in vendor.iterdir() if p.is_file()}
if actual != manifest:
    raise SystemExit('Vendored Lua source checksum mismatch')
for name in ('Colour.hpp', 'BindingDemo.hpp'):
    arduino = root / 'demos/NativeBindings/arduino_ide/NativeBindings' / name
    platformio = root / 'demos/NativeBindings/platformio/src' / name
    if arduino.read_bytes() != platformio.read_bytes():
        raise SystemExit(f'Demo variants differ: {name}')
print(f'Verified {len(actual)} unchanged Lua 5.5.1 files and both demo variants.')
