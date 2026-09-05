# ESPressio-Lua implementation tranche

## Scope
Implement Lua 5.5.1 embedding entirely in ESPressio-Lua on main. Provide reusable native-type Lua definitions; explicitly exposed constructors, fields, methods and properties; per-instance type registration; Lua-owned and borrowed native objects; checked conversion; functions and constants; execution/error and memory contracts; tests, demos and documentation. Halt and notify the user if another repository requires modification. No other repository or branch may be modified.

## Rules
Follow Policy/ABSTRACTION_POLICY.MD, CIRCULAR_DEPENDENCY_POLICY.MD, CROSS_LIBRARY_CHANGES.MD (user's halt instruction takes precedence), DEMO_POLICY.MD, DOCUMENTATION_POLICY.MD, GITHUB_TASK_RESULT_POLICY.MD, LOGGING_INSTRUMENTATION_POLICY.MD, OBSERVER_CALLBACK_POLICY.MD, TRANCHE_TIMEOUT_OR_INTERRUPTION_POLICY.MD, WORKING_BRANCHES.MD (user explicitly selects Lua main). Reuse System allocation abstractions. No platform-specific code in the binding. No circular dependencies. Demos must compile and include Arduino IDE and PlatformIO variants. Document public symbols. Preserve upstream Lua license. Tests must exercise actual Lua 5.5.1. Update this log throughout work; deliver completed handoff then remove it from the repository.

## UTC activity
- 2026-09-05T15:37:00Z: Inspected main: only LICENSE and .gitignore exist. Read all ten organization policies. GitHub connector access works; direct git clone lacks authentication. No external repository changes identified or made. Beginning dependency API inspection and local implementation.
