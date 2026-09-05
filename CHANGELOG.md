# Changelog

## Unreleased

- Embed official Lua 5.5.1 as an unmodified vendored source dependency compiled with C++ exception unwinding.
- Add reusable, frozen native `Type<T>` definitions with explicit constructors, fields, methods and properties.
- Add independent scripting instances, exact-definition receiver validation, Lua-owned construction and application-owned borrowed object registration.
- Add reusable read-only modules, typed native functions, copied constants, checked value converters and host-to-Lua calls.
- Route Lua/native-object storage and binding metadata through existing ESPressio System abstractions; add per-instance reserved-memory and cooperative instruction budgets.
- Contain script/native failures, reject concurrent/reentrant VM entry and prevent unbudgeted Lua-defined GC finalizers.
- Add optional ESPressio Logging instrumentation, documentation, Arduino IDE/PlatformIO demos and native regression/failure-injection tests.
- Add initial 1.0.0 library manifests as the planned release baseline; no release or tag is created.
