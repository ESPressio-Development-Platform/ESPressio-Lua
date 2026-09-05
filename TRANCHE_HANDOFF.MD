# Serial Lua console demo tranche

## Scope and rules
Implement demos/SerialLuaConsole in ESPressio-Lua/main only, with equivalent Arduino IDE and PlatformIO versions. Provide compile-time configurable LED, RGB LED and buzzer pins; initially off native objects; encapsulated Lua type definitions; borrowed object registration; standard Lua print to Serial; ESPressio Command routing of lua run <script>. Build both variants and document wiring, commands and validation. Do not change other repositories or versions.
Follow Policy/DEMO_POLICY.MD, ABSTRACTION_POLICY.MD, DOCUMENTATION_POLICY.MD, GITHUB_TASK_RESULT_POLICY.MD and TRANCHE_TIMEOUT_OR_INTERRUPTION_POLICY.MD. Keep hardware adapters in the demo application. Maintain UTC activity here; archive then remove this handoff on completion.

## Activity
- 2026-09-05T14:54:52.223Z: Inspected existing demos and policies; investigating Command dependency and preserving Lua runtime API.
- 2026-09-05T15:01:18.559Z: Implemented device classes, reusable Lua definitions, variadic print adapter, Command registry adapter and bounded Serial input. Created equivalent Arduino/PlatformIO sources. Command uses its Mesh propagation branch and System/Observable inspected headers. Starting native behavior checks and ESP32 builds; no library API or external repository changed.
