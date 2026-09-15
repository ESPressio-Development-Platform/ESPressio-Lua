# ESPressio Lua
Lua 5.5.1 scripting integration components of the ESPressio Development Platform.

Expose existing C/C++ structs and classes to Lua without changing their native definitions. Describe their Lua-facing interface once, register it with independent scripting instances, and explicitly expose constructors or application-owned objects.

## Development status
This is the initial unreleased implementation. The library manifests retain the planned 1.0.0 baseline; no release or tag is created by the Primitive Platform Redesign work. The canonical branch for the current redesign tranche is `primitives_redesign`. The bundled language runtime is **Lua 5.5.1**.

## ESPressio Development Platform
**ESPressio** is a collection of discrete component libraries built around lightweight implementation, ease of use, object-oriented interfaces and SOLID design. Application-facing abstractions remain separate from hardware implementations. This library uses ESPressio System for allocation and contains no Arduino or ESP-IDF calls in its binding layer.

## License
The ESPressio binding is licensed under [Apache License 2.0](LICENSE). Lua retains its own permissive license; see [third-party notices](THIRD_PARTY_NOTICES.md).

## Namespace
All public binding types live in `ESPressio::Lua`:

| Type | Purpose |
| --- | --- |
| `Type<T>` | Reusable Lua-facing definition of a native type |
| `Instance` | Independent Lua VM, registration, execution and resource accounting |
| `Module` | Reusable read-only namespace of functions and constants |
| `Configuration` | Provider, memory policy, memory/instruction budgets and standard-library selection |
| `Result`, `Status` | Allocation-free protected-operation outcomes |
| `Converter<T>` | Checked, extensible C++/Lua value conversion |
| `Borrowed` | Explicit application-owned object registration |

## Dependencies
Mandatory: ESPressio-System on `primitives_redesign`. The core Lua binding keeps that single mandatory ESPressio dependency.

Optional integrations, including ESPressio-Logging and the Primitive/Command/Event/State tooling adapters, remain opt-in and do not introduce reverse dependencies into their source libraries. During this redesign tranche their corresponding integration workflows use `primitives_redesign`. See [integration contracts](docs/Integration.md).

Lua is vendored at the official `v5.5.1` tag. No runtime download or separate Lua installation is required. C++17 **and C++ exceptions** are required. The bundled runtime uses C++ linkage and exception unwinding; do not link another Lua build or define `LUA_USE_LONGJMP`.

## Platformio.ini
```ini
[env:esp32dev]
platform = espressif32@6.9.0
board = esp32dev
framework = arduino
build_unflags = -std=gnu++11 -fno-exceptions
build_flags = -std=gnu++17 -fexceptions
lib_deps =
    https://github.com/ESPressio-Development-Platform/ESPressio-Lua.git#primitives_redesign
    https://github.com/ESPressio-Development-Platform/ESPressio-System.git#primitives_redesign
```

Private repositories require your normal GitHub checkout credentials. Arduino IDE users should install both repositories as libraries and use the included [Arduino demo](demos/NativeBindings/arduino_ide/NativeBindings), including its ESP32 `build_opt.h`.

## Primitive redesign tooling

Lua is a consumer of the final Primitive-family contracts, not a semantic owner or alternate registry.

- `ESPressio_LuaPrimitiveDiscovery.hpp` projects a frozen `Primitive::TypeDirectoryView` into Lua-facing discovery without importing family behavior into the generic discovery adapter.
- Family-specific adapters consume the final descriptor/schema surfaces. A Type being discoverable does **not** authorize an operation.
- Dynamic Commands are schema/factory constructed and submitted through the final Command admission/runtime path. Lua does not bypass admission or manufacture an Event to execute a Command.
- Event operations use the final Event APIs and retain Event delivery/admission policy.
- Generic State exposure is read/inspect only; discovering State metadata cannot grant owner-write authority.
- Dynamic names, scripts, serialized inputs and construction storage are bounded before parse/construction. Invalid input returns an explicit tooling/Lua failure rather than an unbounded heap fallback, raw reinterpretation or exception-driven retry loop.
- The focused `primitives-redesign-discovery`, `-command`, `-event` and `-state` workflows compile and execute these optional adapter contracts against the final redesign branches.

## Usage Examples
Define an ordinary native type:

```cpp
struct Colour {
    int red, green, blue;
    Colour(int r, int g, int b) : red(r), green(g), blue(b) {}
    void clear() { red = green = blue = 0; }
};
```

Describe and register its Lua interface:

```cpp
#include <ESPressio_Lua.hpp>
namespace Lua = ESPressio::Lua;

Colour statusColour{0, 255, 0}; // Must outlive the scripting instance.
Lua::Type<Colour> colourType{"Colour"};
colourType.constructor<int, int, int>()
    .field("red", &Colour::red)
    .field("green", &Colour::green)
    .field("blue", &Colour::blue)
    .method("clear", &Colour::clear);

Lua::Instance script;
auto result = script.initializationResult();
if (!result) { /* Report result.message and stop initialization. */ return; }
result = script.registerType(colourType);
if (!result) { /* Report result.message. */ return; }
result = script.registerInstance("statusColour", colourType, statusColour, Lua::Borrowed);
if (!result) { /* Report result.message. */ return; }
result = script.execute(R"lua(
    local colour = Colour(0, 128, 255)
    colour.red = 64
    colour:clear()
    statusColour.blue = 128
)lua");
if (!result) { /* Report result.message. */ return; }
```

The local `colour` is Lua-owned; its C++ destructor runs on garbage collection or VM shutdown. `statusColour` remains application-owned. Registering a type without `.constructor<...>()` still allows borrowed objects but does not allow Lua construction.

The [native-bindings demo](demos/NativeBindings/README.md) adds computed properties, read-only properties, a module, and host-to-Lua function calls. The [Serial Lua console demo](demos/SerialLuaConsole/README.md) exposes application-owned LED, RGB LED and buzzer objects and executes operator-provided Lua through an ESPressio Command. [Binding guide](docs/Bindings.md) covers reuse, alternative views, overloads, conversions and errors.

### Calling Lua objects and functions from C++

The current `Instance` API provides `execute`, `call` and `readGlobal`.
`call` passes typed arguments to a **global function**, but discards its Lua return values; its `Result` reports execution success or failure. There is no public Lua-object handle or direct method-call/return-value API yet. The example below uses a small Lua adapter to resolve a global object by name, call its method, and place the returned value in a global that C++ can read.

```cpp
#include <ESPressio_Lua.hpp>
#include <cstdio>
#include <string_view>

namespace Lua = ESPressio::Lua;

int main() {
    Lua::Instance script;
    auto check = [](const Lua::Result& result) {
        if (!result) std::fprintf(stderr, "%s\n", result.message);
        return static_cast<bool>(result);
    };
    if (!check(script.initializationResult())) return 1;

    if (!check(script.execute(R"lua(
        calculator = { offset = 10 }
        function calculator:add(a, b)
            return self.offset + a + b
        end
        function multiply(a, b)
            return a * b
        end
        function hostCallMethod(objectName, methodName, a, b)
            hostReturnValue = nil
            local object = _G[objectName]
            assert(object ~= nil, "Global Lua object was not found")
            local method = object[methodName]
            assert(type(method) == "function", "Object member is not a function")
            hostReturnValue = method(object, a, b)
        end
        function hostCallGlobal(functionName, a, b)
            hostReturnValue = nil
            local fn = _G[functionName]
            assert(type(fn) == "function", "Global is not a function")
            hostReturnValue = fn(a, b)
        end
    )lua"))) return 1;

    if (!check(script.call("hostCallMethod",
                           std::string_view{"calculator"},
                           std::string_view{"add"}, 4, 7))) return 1;
    int methodReturn = 0;
    if (!check(script.readGlobal("hostReturnValue", methodReturn))) return 1;
    std::printf("Method returned: %d\n", methodReturn); // 21

    if (!check(script.call("hostCallGlobal",
                           std::string_view{"multiply"}, 6, 7))) return 1;
    int globalReturn = 0;
    if (!check(script.readGlobal("hostReturnValue", globalReturn))) return 1;
    std::printf("Global returned: %d\n", globalReturn); // 42
    return 0;
}
```

`method(object, a, b)` is the dynamic-name equivalent of `calculator:add(a, b)`: the object is the explicit first argument (`self`). `_G[objectName]` looks up one exact global name, not a dotted path; Lua `local` variables are not global objects.

`readGlobal` performs checked conversion into the requested C++ type. Only read the output after both the call and conversion succeed. Keep a call/read pair on one application execution context, or hold an application lock across both, because they are separate operations and another call could overwrite the shared Lua result value.

### Calling from a C translation unit

ESPressio-Lua's public interface requires C++17 and its bundled Lua runtime uses C++ linkage. A pure C source file cannot include `ESPressio_Lua.hpp`. Put a C-compatible entry point in a `.cpp` bridge and link the final application with the C++ linker. Do not allow a C++ exception to cross that C ABI boundary. For repeated calls or persistent object state, keep an application-owned `Instance` alive in the C++ bridge and expose explicit C lifecycle/operation functions.

## Execution and Resource Contracts
Each `Instance` owns an independent Lua state. Entry is serialized by rejection: concurrent or reentrant operations return `Status::Busy`, so the application can queue work using its chosen ESPressio execution abstractions. Calls execute synchronously on the calling task; there is no hidden thread or scheduler.

The default budget is 256 KiB of reserved Lua/native-object storage and 100,000 Lua instructions per execution operation. Native calls must remain bounded; an instruction hook cannot interrupt a blocking C++ function. See [runtime contracts](docs/Runtime.md) for memory-accounting boundaries, lifetime rules and capability restrictions.

This initial API supports registered constructors with distinct arities, fields, methods, properties, modules, constants, typed functions, text execution and global reads. It does not implement automatic native-pointer conversion, inheritance/casts, arbitrary operators, same-arity overload resolution, shared native ownership, asynchronous event bridges, script hot reload, or coroutine scheduling. These need explicit contracts before adding them; unsupported value conversions fail at compile time.

## Building and Testing
```sh
cmake -S . -B build -DESPRESSIO_SYSTEM_DIR=/absolute/path/to/ESPressio-System
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Add `-DESPRESSIO_LUA_SANITIZE=ON` for AddressSanitizer/UndefinedBehaviorSanitizer. Tests execute the bundled Lua 5.5.1 runtime and cover ownership/finalization, reusable definitions, view isolation, conversion failures, native errors, concurrency rejection, budgets and allocation-failure recovery. The focused Primitive redesign workflows add discovery and family-adapter coverage. See [validation notes](docs/Validation.md).

## Extensions
Add `Converter<T>` specializations in your application or in adapters owned by this repository. Keep Lua-specific references out of upstream ESPressio libraries. See [extension contracts](docs/Integration.md).
