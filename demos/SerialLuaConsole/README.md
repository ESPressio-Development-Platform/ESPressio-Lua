# Serial Lua Console Demo

This demo lets an operator execute Lua on an ESP32 through its USB Serial
console. It demonstrates application-owned output devices, reusable Lua type
definitions, standard Lua `print(...)` redirected through C++, and an
ESPressio Command handler registered at:

```text
lua run <script>
```

The Lua VM persists between commands. Scripts can therefore define globals and
functions in one command and use them later. Treat the console as a privileged
development interface: it intentionally lets anyone with Serial access operate
every capability registered with this Lua instance.

## Hardware

The defaults target a classic ESP32 development board:

| Device | Default GPIO | Electrical expectation |
| --- | ---: | --- |
| LED | 23 | LED with a suitable series resistor |
| RGB red | 25 | PWM-capable channel with a suitable resistor/driver |
| RGB green | 26 | PWM-capable channel with a suitable resistor/driver |
| RGB blue | 27 | PWM-capable channel with a suitable resistor/driver |
| Passive buzzer | 32 | Passive buzzer through a suitable driver when required |

All outputs are explicitly initialized OFF. The LED and RGB LED support
active-high and active-low wiring. The buzzer example assumes an active-high
passive buzzer. Check your board's electrical limits; GPIO pins must not drive
loads beyond their rated current.

Override the pins and polarity at compile time:

| Macro | Default |
| --- | ---: |
| `DEMO_LED_PIN` | 23 |
| `DEMO_RGB_RED_PIN` | 25 |
| `DEMO_RGB_GREEN_PIN` | 26 |
| `DEMO_RGB_BLUE_PIN` | 27 |
| `DEMO_BUZZER_PIN` | 32 |
| `DEMO_LED_ACTIVE_LOW` | 0 |
| `DEMO_RGB_ACTIVE_LOW` | 0 |

Every output must use a distinct, output-capable GPIO. PlatformIO users can
uncomment and edit the `build_flags` examples in `platformio.ini`. Arduino
IDE users can edit the macro definitions near the top of `DemoConfig.hpp`
before compiling.

## PlatformIO

Open `platformio` as the project directory, authenticate GitHub access for the
private ESPressio repositories, then build and upload:

```sh
pio run
pio run --target upload
pio device monitor --baud 115200
```

The configuration pins ESPressio dependencies to their current structural
realignment branches. It also fixes Arduino-ESP32 at the version supplied by
`espressif32@6.9.0`; the demo checks for the corresponding 2.x LEDC API.

## Arduino IDE

Install ESP32 Arduino core 2.0.17 and install ESPressio-Lua plus the ESPressio
Command dependency chain listed in the PlatformIO configuration. Open:

```text
arduino_ide/SerialLuaConsole/SerialLuaConsole.ino
```

Select an ESP32 board, upload, then open Serial Monitor at 115200 baud with a
newline or CRLF line ending. Keep `build_opt.h` beside the sketch: it enables
C++17 and C++ exceptions as required by ESPressio-Lua.

## Lua-visible objects

The C++ application creates one `Led`, `RgbLed` and `Buzzer`, then exposes
those existing instances as borrowed Lua globals. Lua cannot construct hardware
objects because their type definitions expose no constructors.

| Global | Readable interface | Operations |
| --- | --- | --- |
| `led` | `isOn` | `on()`, `off()`, `toggle()`; assign `isOn` |
| `rgb` | `red`, `green`, `blue`, `isOn` | assign channels; `setColour(r,g,b)`, `off()` |
| `buzzer` | `isOn`, `frequency` | `play(hz)`, `off()` |

RGB channel values are checked in the inclusive range 0–255. Buzzer frequency
is checked in the inclusive range 20–20000 Hz. `play` starts a continuous,
nonblocking tone; always call `buzzer:off()` when finished.

## Operator examples

```text
lua run print(led.isOn, rgb.isOn, buzzer.isOn)
lua run led:on()
lua run led:toggle(); print("LED on:", led.isOn)
lua run rgb:setColour(0, 128, 255)
lua run rgb.red = 64; print(rgb.red, rgb.green, rgb.blue)
lua run buzzer:play(440); print("Frequency:", buzzer.frequency)
lua run led:off(); rgb:off(); buzzer:off()
```

`print(...)` applies Lua's standard `tostring` conversion, separates values
with tabs and emits a newline through a registered C++ Serial writer. Each
operator command receives a bounded 4096-byte print allowance.

Only the first two space-delimited words are interpreted as the Command path.
Everything after the separator following `lua run` is passed unchanged to
`Instance::execute`, preserving Lua quotes and backslashes.

## Structure

- `OutputDevices.hpp` owns the demo-only Arduino GPIO/PWM implementations.
- `LuaDeviceTypes.hpp` encapsulates each Lua-facing type definition in a
  factory, making each definition registerable with one call.
- `LuaSerialPrint.hpp` installs the C++ writer and Lua `print(...)`.
- `LuaCommandHandler.hpp` owns the ESPressio Command registry and `lua/run`.
- `SerialConsole.hpp` incrementally reads bounded lines without blocking.
- `SerialLuaDemo.hpp` owns objects in shutdown-safe order and registers them.

The Arduino IDE and PlatformIO directories contain equivalent source files.

## Limits

Input is bounded to 1023 bytes plus its terminator. An oversized or binary line
is drained and rejected without executing its truncated prefix. Lua execution
also retains ESPressio-Lua's memory and instruction budgets. Serial input is
processed synchronously in `loop()`; a long valid script can therefore delay
other application work until it completes or reaches its instruction limit.

No physical-device test is implied by a successful build. Confirm pin polarity,
load-driving circuitry, RGB response and buzzer behavior on your actual board.
