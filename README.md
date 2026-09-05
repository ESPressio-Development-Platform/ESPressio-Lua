# ESPressio Lua
Lua 5.5.1 scripting integration components of the ESPressio Development Platform.

Expose existing C/C++ structs and classes to Lua without changing their native definitions. Describe their Lua-facing interface once, register it with independent scripting instances, and explicitly expose constructors or application-owned objects.

## Latest Stable Version
This is the initial, unreleased implementation on `main`. The library manifests use the planned 1.0.0 baseline; no release or tag is created by this implementation. The bundled language runtime is **Lua 5.5.1**.

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
Mandatory: [ESPressio-System](https://github.com/ESPressio-Development-Platform/ESPressio-System/tree/structural_realignment), pinned to `structural_realignment` in manifests, demos and CI. No other ESPressio repository requires changes.

Optional: [ESPressio-Logging](https://github.com/ESPressio-Development-Platform/ESPressio-Logging/tree/structural_realignment), enabled by `ESPRESSIO_LUA_ENABLE_LOGGING=1`. See [integration contracts](docs/Integration.md) for its dependency branches and build requirements. No reverse dependency is introduced.

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
    https://github.com/ESPressio-Development-Platform/ESPressio-Lua.git#main
    https://github.com/ESPressio-Development-Platform/ESPressio-System.git#structural_realignment
```

Private repositories require your normal GitHub checkout credentials. Arduino IDE users should install both repositories as libraries and use the included [Arduino demo](demos/NativeBindings/arduino_ide/NativeBindings), including its ESP32 `build_opt.h`.

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

The [complete compilable demos](demos/NativeBindings/README.md) add computed properties, read-only properties, a module, and host-to-Lua function calls. [Binding guide](docs/Bindings.md) covers reuse, alternative views, overloads, conversions and errors.

### Calling Lua objects and functions from C++

The current `Instance` API provides `execute`, `call` and `readGlobal`.
`call` passes typed arguments to a **global function**, but discards its Lua
return values; its `Result` reports execution success or failure. There is no
public Lua-object handle or direct method-call/return-value API yet. The example
below uses a small Lua adapter to resolve a global object by name, call its
method, and place the returned value in a global that C++ can read.

This complete C++17 example uses the bundled runtime:

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
        -- A global Lua object (a table with a method).
        calculator = { offset = 10 }
        function calculator:add(a, b)
            return self.offset + a + b
        end

        -- A global Lua function, independent of the object.
        function multiply(a, b)
            return a * b
        end

        -- Application-defined adapters, not built-in ESPressio APIs.
        function hostCallMethod(objectName, methodName, a, b)
            hostReturnValue = nil
            local object = _G[objectName] -- Resolve the global by name.
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

    // Resolve calculator by name and call calculator:add(4, 7).
    // Names and values cross the binding as arguments, not generated Lua text.
    if (!check(script.call("hostCallMethod",
                           std::string_view{"calculator"},
                           std::string_view{"add"}, 4, 7))) return 1;
    int methodReturn = 0;
    if (!check(script.readGlobal("hostReturnValue", methodReturn))) return 1;
    std::printf("Method returned: %d\n", methodReturn); // 21

    // Execute a global function directly when its return value is not needed.
    if (!check(script.call("multiply", 6, 7))) return 1;

    // Execute the same global function and retrieve its return value.
    if (!check(script.call("hostCallGlobal",
                           std::string_view{"multiply"}, 6, 7))) return 1;
    int globalReturn = 0;
    if (!check(script.readGlobal("hostReturnValue", globalReturn))) return 1;
    std::printf("Global returned: %d\n", globalReturn); // 42
    return 0;
}
```

`method(object, a, b)` is the dynamic-name equivalent of
`calculator:add(a, b)`: the object is the explicit first argument (`self`).
For a table function declared with dot syntax that does not accept `self`, use
`method(a, b)` instead. `_G[objectName]` looks up one exact global name, not a
dotted path; Lua `local` variables are not global objects.

`readGlobal` performs checked conversion into the requested C++ type. Only read
the output after both the call and conversion succeed. These adapters capture
one return value; additional Lua return values are discarded. The example uses
application-owned global names `hostCallMethod`, `hostCallGlobal` and
`hostReturnValue`; reserve them for this purpose. Keep the call/read pair on one
application execution context (or hold an application lock across both), because
they are two separate operations and another call could overwrite the value.
Object resolution stays inside Lua and occurs on every adapter call; this does
not retain a native handle to the object.

### Calling from a C translation unit

ESPressio-Lua's public interface requires C++17 and its bundled Lua runtime uses
C++ linkage. A pure C source file cannot include `ESPressio_Lua.hpp` or directly
link to this runtime through an ordinary Lua C build. Use a C-compatible entry
point implemented in a `.cpp` file. For example, this minimal wrapper creates a
VM, defines a global function, passes two C integers to it and returns its result:

```cpp
// lua_bridge.cpp -- compile as C++17 with exceptions and link ESPressio-Lua.
#include <ESPressio_Lua.hpp>

extern "C" int espressio_lua_multiply(int a, int b, int* output) noexcept {
    if (!output) return 0;
    try {
        ESPressio::Lua::Instance script;
        if (!script.initializationResult()) return 0;
        if (!script.execute(R"lua(
            function multiply(a, b) return a * b end
            function hostMultiply(a, b) hostReturnValue = multiply(a, b) end
        )lua")) return 0;
        if (!script.call("hostMultiply", a, b)) return 0;
        int value = 0;
        if (!script.readGlobal("hostReturnValue", value)) return 0;
        *output = value;
        return 1;
    } catch (...) {
        return 0; // Never propagate a C++ exception into the C caller.
    }
}
```

```c
/* caller.c -- compile as C; link the application using the C++ linker. */
extern int espressio_lua_multiply(int a, int b, int* output);

int main(void) {
    int value = 0;
    if (!espressio_lua_multiply(6, 7, &value)) return 1;
    return value == 42 ? 0 : 1;
}
```

The C wrapper returns `1` on success and `0` on failure, leaving `*output`
unchanged on failure. For repeated calls or object state that must persist,
keep an application-owned `Instance` alive in the C++ bridge and expose C entry
points for its lifecycle and operations. Its object-method entry point can use
the same `hostCallMethod` adapter above.

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

Add `-DESPRESSIO_LUA_SANITIZE=ON` for AddressSanitizer/UndefinedBehaviorSanitizer. Tests execute the bundled Lua 5.5.1 runtime and cover ownership/finalization, reusable definitions, view isolation, conversion failures, native errors, concurrency rejection, budgets and allocation-failure recovery. See [validation notes](docs/Validation.md).

## Extensions
Add `Converter<T>` specializations in your application or in adapters owned by this repository. Keep Lua-specific references out of upstream ESPressio libraries. See [extension contracts](docs/Integration.md).
