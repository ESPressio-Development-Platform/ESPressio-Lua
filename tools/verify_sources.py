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
        raise SystemExit(f'NativeBindings demo variants differ: {name}')

serial_sources = (
    'DemoConfig.hpp',
    'LuaCommandHandler.hpp',
    'LuaDeviceTypes.hpp',
    'LuaSerialPrint.hpp',
    'OutputDevices.hpp',
    'SerialConsole.hpp',
    'SerialLuaDemo.hpp',
)
for name in serial_sources:
    arduino = root / 'demos/SerialLuaConsole/arduino_ide/SerialLuaConsole' / name
    platformio = root / 'demos/SerialLuaConsole/platformio/src' / name
    if arduino.read_bytes() != platformio.read_bytes():
        raise SystemExit(f'SerialLuaConsole demo variants differ: {name}')

print(
    f'Verified {len(actual)} unchanged Lua 5.5.1 files, NativeBindings equivalence, '
    f'and {len(serial_sources)} SerialLuaConsole paired sources.'
)
