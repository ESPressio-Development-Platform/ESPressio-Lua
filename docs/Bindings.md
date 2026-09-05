# Native binding guide

## Separate native types, Lua definitions and scripting instances
`Type<T>` describes an unmodified native type. `Instance::registerType` installs that interface in one VM. `registerInstance` exposes a particular native object through that interface. Constructed Lua objects use the same interface.

```cpp
Lua::Type<Controller> controls{"Controller"};
controls.method("clear", &Controller::clear)
    .property("brightness", &Controller::brightness, &Controller::setBrightness)
    .readOnlyProperty("count", &Controller::count);
```

Members are explicitly opt-in. Native public members that are not registered remain inaccessible. `field` exposes direct read/write access (const fields are read-only); `readOnlyField` deliberately hides assignment. Properties call native getters/setters. Read-only is an access rule, not a promise that a getter has no side effects.

## Constructors and ownership
`.constructor<A...>()` exposes `Native(A...)` as `TypeName(...)` in Lua. Register multiple constructors only with distinct argument counts. Same-arity constructor overloads are rejected at definition time instead of guessing between Lua values. All arguments are checked before native construction. Throwing constructors release their allocation and return a Lua error. Lua-owned destructors must be `noexcept`.

Objects constructed by Lua are allocated through the instance's captured System provider and charged to its memory budget. Lua finalization destroys each object exactly once and returns storage to that provider. Alignment requirements, including over-aligned native types, are forwarded to the provider.

`registerInstance("name", definition, native, Lua::Borrowed)` is non-owning. The ownership marker can be omitted; it still means borrowed. The native object must outlive the **entire VM**, including all shutdown finalizers. Removing a Lua global does not invalidate references retained elsewhere. This initial API does not offer unregister, early invalidation or shared ownership.

Use declaration order deliberately: declare application-owned objects before the `Instance`, so reverse C++ destruction closes the VM first. Native object members can themselves own memory; that memory follows the native type's allocator contract, not the Lua VM's accounting.

## Reuse and alternative views
Copying a definition shares its identity. First registration freezes the definition and every copy. Further mutation throws `BindingError`. Freeze explicitly before sharing a definition between host threads; concurrent definition building/first registration is not supported. A failed registration may still freeze and retain a definition because partial Lua closures must remain valid; retrying registration is supported.

One definition can be registered with multiple VMs. VMs retain it independently, so a local builder may leave scope immediately after registration. Registering the same definition again in the same VM is idempotent. Registering a different definition under an occupied name fails.

Create separate `Type<T>` objects with different names to expose different interfaces to the same native type. Native method receiver validation checks the **exact definition**, not just `T`. A method extracted from a control view cannot be called with a monitoring-view receiver. Application code can still deliberately register both views of the same native object.

## Methods and argument conversion
Use `object:method(...)` in Lua; `object.method(object, ...)` is equivalent. An extracted method requires a receiver of its original definition. Missing, extra or invalid arguments fail before entering native code.

For overloaded C++ methods, provide an explicit pointer cast, for example:

```cpp
type.method("setInt", static_cast<void (Native::*)(int)>(&Native::set));
```

Const and noexcept member functions work. Mutable reference parameters need an explicit adapter: converted Lua scalar/string values are copies, not native lvalues. Typed lambdas and free functions are supported by `registerFunction`; generic/overloaded call operators need a typed wrapper. Mutable callable state can be placed in a captured application object whose lifetime and synchronization are managed by the application.

Supported conversions:

| C++ type | Lua representation / validation |
| --- | --- |
| `bool` | Boolean only |
| Integral types | Lua integer only; signedness/range checked |
| Floating types | Finite number; overflow rejected; normal floating rounding remains possible |
| Enums | Checked underlying integer; enum-domain membership is application-defined |
| `std::string`, System strings | Owning binary-safe string |
| `std::string_view` | Borrowed argument bytes valid only during the call; outputs copied |
| `const char*` | String or nil; embedded NUL rejected on input; outputs copied |
| `std::optional<T>` | nil or converted value; an omitted argument is not equivalent to explicit nil |

Unsigned values above Lua's signed integer maximum are rejected. Lua strings are not coerced into numbers; numbers are not coerced into strings. Raw native pointers/references, nested native values and inheritance have no automatic conversion. This deliberately prevents an implicit return conversion from exposing a more permissive view. Explicit adapters must define the output view and lifetime.

## Modules, functions and constants
```cpp
Lua::Module app{"app"};
app.constant("Maximum", 255)
   .constant("ModeName", "training")
   .function("double", [](int value) { return value * 2; });
auto result = script.registerModule(app);
```

Lua uses `app.double(32)` and `app.Maximum`. Modules are immutable userdata namespaces; they cannot be changed through table operations. Module string constants are copied into System-backed definition storage. Callables and their captures follow the same lifetime rules as global functions. Nested module/type registration is not implemented.

`registerConstant` copies a value into a VM as a protected global. A named global constant cannot be nil. Enum members can be registered as integer-valued constants in a module. Module constants may represent nil because module lookup uses its definition, not nil as a registration marker.

Registered global names cannot be overwritten by ordinary Lua assignment. They live in an export table behind the global environment's locked metatable; `pairs(_G)` does not enumerate them. Names may contain dots, but a dot in a binding name is literal, not automatic namespace creation: use a `Module` for namespace syntax.

## Host calls and errors
Check every `Result`. `execute` compiles text and discards returned values; `call` invokes a named Lua function with converted arguments and discards its returns. `readGlobal` converts a global into an output variable. For return values, let a script function update a global and read it, or add an application-specific adapter.

Builder misuse throws `BindingError`; allocation while building definitions can throw `std::bad_alloc`. VM initialization and protected operations return `Result`. Standard native exceptions become Lua errors with their message; non-standard exceptions are contained by Lua's C++ protected-call mechanism and reported as a generic runtime failure. Native side effects before an error are not rolled back.
