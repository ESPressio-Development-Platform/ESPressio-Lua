# Integration and extension contracts

## Dependency ownership
Only ESPressio-Lua owns Lua bindings and vendored interpreter code. Mandatory upstream dependency: ESPressio-System at `structural_realignment`. Its existing `IMemoryProvider`, `MemoryPolicy`, container aliases and `MakeShared` are used without changes. No generic language abstraction or Lua adapter is added to another repository.

The optional logging switch `ESPRESSIO_LUA_ENABLE_LOGGING=1` includes ESPressio-Logging and emits protected-operation failures in category `ESPressio.Lua`. Disabled is the default and introduces no Logging include or link requirement. Set this macro consistently across translation units. Logging's own level switches still apply.

For optional logging, install/pin all of these existing working branches:

| Repository | Branch |
| --- | --- |
| ESPressio-System | structural_realignment |
| ESPressio-Logging | structural_realignment |
| ESPressio-Observable | structural_realignment |
| ESPressio-Timing | structural_realignment |
| ESPressio-Units | structural_realignment |

The selected existing Logging/Timing/Units dependency chain includes Arduino-facing types. Applications enabling that optional integration must satisfy those libraries' platform requirements. The default Lua binding has no such dependency. Timing's include directory must precede System's if manually assembling include paths, because those working branches contain overlapping legacy header names. PlatformIO should resolve the real packages normally.

## Value converter extension
Specialize `ESPressio::Lua::Converter<T>` with:

```cpp
static T read(lua_State* state, int index);
static void push(lua_State* state, const T& value);
```

`read` validates and returns a value; do not coerce invalid input silently. `push` leaves exactly one value on the stack. Throw a standard exception for invalid values or use Lua's supported error API within a protected operation. Do not retain borrowed Lua pointers, alter the VM's allocator/hook/environment, manually release native wrappers, or catch Lua's private unwind exception. Any temporary native allocations must be RAII-managed.

For ESPressio Units/identifiers/Serializable values, an adapter must choose a representation and define ranges, ownership and loss of precision. The binding does not silently serialize native objects, guess a Lua type from a pointer, or expose another view on return. If a chosen representation requires serialization, consume ESPressio-Serializable's abstractions rather than implementing a competing serialization layer.

## Internal responsibilities
`ESPressio_LuaValue.hpp` owns conversion and binding errors. `ESPressio_LuaType.hpp` owns reusable native type descriptors. `ESPressio_LuaModule.hpp` owns reusable namespace descriptors. `detail/ESPressio_LuaBinding.hpp` owns invocation, userdata identity and allocation adaptation. `ESPressio_LuaInstance.hpp` owns state, publication, execution and lifetime retention. `ESPressio_LuaRuntime.cpp` is the C++ amalgamation wrapper around unmodified Lua sources.

Definition/callable storage uses System allocators; `std::function` is not used as an allocation-hiding wrapper. Template instantiation erases each native callable behind a small virtual interface. Native members are looked up linearly in the frozen definition, trading small descriptor memory for lookup cost; profile actual workloads before replacing this strategy.

Do not add upstream reverse dependencies for convenience. If a feature cannot be implemented using existing dependency contracts, halt and seek the repository owner's instruction before changing another repository.
