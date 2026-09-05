# Native type binding demo

Both versions execute the same example and report `Native colour: 42, 255, 128`.

- `arduino_ide/NativeBindings`: open `NativeBindings.ino`; install ESPressio-Lua/main and ESPressio-System/structural_realignment as libraries. Select an ESP32 board. Keep `build_opt.h` next to the sketch: Arduino ESP32 2.0.17's build recipe reads it to enable C++17 and exceptions. Other board cores may require equivalent compiler settings.
- `platformio`: open this folder and run `pio run`; the manifest pins espressif32 6.9.0 / Arduino ESP32 2.0.17. Provide normal GitHub credentials for private dependencies.

`Colour.hpp` is the unmodified native application type. `BindingDemo.hpp` owns the native object, Lua type definition and VM, exposes constructors/members/properties, registers a module and executes the script. The tiny sketch/main handles only platform serial output. There are no hardware operations in the binding example itself.

All registration/execution results are checked. Declaration order ensures the borrowed object outlives the VM. `run` is an initialization demo intended to execute once; it must not rebuild a frozen definition on each loop iteration.

The two copies of `Colour.hpp` and `BindingDemo.hpp` are intentional, allowing either project folder to stand alone. CI verifies they stay identical. The native `demo_test` also compiles and executes the shared demonstration logic without Arduino.
