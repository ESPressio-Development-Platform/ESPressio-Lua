# Integration and extension contracts

## Dependency ownership
Only ESPressio-Lua owns Lua bindings and vendored interpreter code. Mandatory upstream dependency: ESPressio-System at `primitives_redesign`. Its existing allocation, memory-policy and platform contracts are consumed without moving Lua semantics upstream. No generic language abstraction or Lua adapter is added to another repository.

The optional logging switch `ESPRESSIO_LUA_ENABLE_LOGGING=1` includes ESPressio-Logging and emits protected-operation failures in category `ESPressio.Lua`. Disabled is the default and introduces no Logging include or link requirement. Set this macro consistently across translation units. Logging's own level switches still apply.

For optional logging during the Primitive Platform Redesign, use the corresponding `primitives_redesign` branches:

| Repository | Branch |
| --- | --- |
| ESPressio-System | primitives_redesign |
| ESPressio-Logging | primitives_redesign |
| ESPressio-Observable | primitives_redesign |
| ESPressio-Timing | primitives_redesign |
| ESPressio-Units | primitives_redesign |

The selected Logging/Timing/Units dependency chain may include target-facing types depending on the concrete platform. Applications enabling optional integration must satisfy those libraries' platform requirements. The default Lua binding has no such dependency. PlatformIO should resolve the real packages normally rather than relying on manually ordered legacy include paths.

## Primitive-family adapters

The Primitive Platform Redesign keeps Lua as a dynamic tooling surface rather than a semantic owner. Optional adapter integrations follow the same boundaries as Web and Serial tooling:

- discover Types through a frozen `Primitive::TypeDirectoryView`; Lua does not create or mutate a competing Type registry;
- consume final family descriptor/schema metadata when exposing dynamic Types;
- construct and admit Commands through the final typed Command descriptor/runtime path; a discoverable Command is not automatically authorized;
- expose Event operations only through final Event APIs and their delivery/admission policy;
- expose generic State as read/inspect only. Arbitrary Lua code cannot acquire State-owner authority by discovering a State descriptor;
- bound names, source text, serialized input and construction storage before parse/construction; malformed or unsupported input returns an explicit Lua/tool error rather than falling back to unbounded allocation, raw reinterpretation or exception-driven retry;
- keep each optional adapter dependency directed into Lua/tooling. Primitive, Command, Event and State do not depend on ESPressio-Lua.

The focused `primitives-redesign-discovery`, `-command`, `-event` and `-state` workflows are the executable integration contracts for those optional surfaces.

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
