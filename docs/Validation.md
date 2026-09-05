# Validation

## Executed locally, 2026-09-05

- GCC 13.3 / C++17 CMake build with AddressSanitizer and UndefinedBehaviorSanitizer: both `binding_tests` and `demo_test` passed.
- Tests exercised actual bundled Lua 5.5.1, not a Lua API stub. They covered native construction/destruction, borrowed ownership, over-alignment, frozen definition lifetime, multiple VMs/views, functions/modules/constants, conversion range/type errors, standard and non-standard native exceptions, recovery, concurrent/reentrant entry, instruction limits and rejection of script-defined GC finalizers.
- Allocation injection covered 160 initialization failure positions and 45 type-registration failure positions with retry and complete shutdown accounting. A tracking System provider checked every release's original byte size and alignment; all tracked allocations were reclaimed.
- The container cannot support LeakSanitizer's process inspection. Local sanitizer tests used `ASAN_OPTIONS=detect_leaks=0`; AddressSanitizer and UndefinedBehaviorSanitizer remained enabled. CI leaves LeakSanitizer enabled on a normal runner.
- The PlatformIO `main.cpp` demo and Arduino `.ino` demo both compiled and linked against the ESP32 Arduino core using PlatformIO Core 6.1.19, espressif32 6.9.0, Arduino ESP32 2.0.17 and Xtensa GCC 8.4.0. Each produced firmware with 21,552 bytes static RAM and 444,737 bytes flash. Static RAM figures exclude the runtime Lua heap.
- The Arduino IDE GUI was not launched. Its sketch was built with the same ESP32 Arduino toolchain via PlatformIO; the upstream Arduino ESP32 2.0.17 recipe was checked for `build_opt.h` support.
- Optional Logging enabled: compiled and linked against the actual selected System, Logging, Timing, Units and Observable headers, then executed the native smoke example. Logging's existing Arduino test shim supplied host Arduino declarations; no dependency source was modified.
- The source verifier checked all 63 vendored files against recorded SHA-256 values and verified the duplicate demo implementation files were identical.

No physical-device execution was performed.

## CI
The workflow builds native sanitizer regressions and the ESP32 PlatformIO demo. All System references use `structural_realignment`.

For private cross-repository checkout, GitHub may require an organization/repository secret named `ESPRESSIO_REPOSITORY_TOKEN` with read access to ESPressio-System. The workflow falls back to the normal GitHub token for installations where it has sufficient access. This implementation does not create or change credentials, organization settings or another repository.

Local test outcomes are independent of GitHub runner availability, billing limits or checkout credentials. Consult the workflow result for the current commit before treating CI as passed.

### First GitHub run

[Run 33971612429](https://github.com/ESPressio-Development-Platform/ESPressio-Lua/actions/runs/33971612429), commit `ebd525b608e1f5b298a31034d6ee9bd1e7db544c`: both jobs ended in failure after approximately two seconds, with no executed steps. Job logs were unavailable (404). The accessible metadata does not establish the precise startup failure reason; this is not recorded as a test failure or a passing CI run. Local executed verification above remains the evidence for this implementation. No rerun or organization/credential change was attempted.
