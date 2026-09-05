# Runtime and resource contracts

## State and execution
An `Instance` owns one independent Lua 5.5.1 state. It cannot be copied or moved. Definitions are shared host metadata; Lua heaps, global environments and userdata are per-instance. Borrowing the same native object into two VMs deliberately shares that native object; the application must coordinate access across VMs.

`execute`, `call`, registration, global reads and collection reject concurrent/reentrant entry with `Busy`. No mutex wait, OS thread or scheduler is introduced. Destruction requires external quiescence; destroying an instance during another call is invalid. Memory-statistic reads also require quiescence. If integration needs task scheduling, queue work through the application's existing ESPressio execution abstractions.

Lua calls are synchronous. They cannot yield across native bindings. Coroutines are not opened in this initial API. Event, Command and State bridges are not implicitly installed; callbacks invoked from a future bridge must enter through the instance's execution context and obey the upstream Observable contracts.

## Lua error unwinding
The pinned Lua sources are compiled as C++, using upstream Lua's C++ exception mechanism. C++ destructors therefore run on a Lua error or allocation failure. Building Lua as C or defining `LUA_USE_LONGJMP` would violate this contract. Do not substitute a system Lua library. Both binding and runtime require exceptions enabled.

Every allocating Lua API operation happens inside a protected trampoline. Native argument temporaries and constructor storage are released on errors. Lua private unwind exceptions are allowed to propagate; the binding catches only standard native exceptions when generating an error message. Results use fixed-size 512-byte message storage, avoiding another allocation just to report VM memory exhaustion. Messages may be truncated and do not include an automatically generated traceback.

## Memory
`Configuration::memoryProvider` defaults to the System provider active when the instance is constructed. That exact provider remains responsible for every matching release, even if the global provider later changes. The provider must outlive the instance and honor requested alignment and policy. Unsupported policies fail initialization.

`memoryLimitBytes` counts actual reserved Lua allocation blocks, their small bookkeeping headers, and storage for Lua-constructed native objects. A growing Lua allocation temporarily holds both old and replacement blocks; the peak/budget includes that overlap. Shrinking keeps reserved capacity, as Lua requires shrinking allocations to succeed. `memoryUsed` therefore reports reserved storage rather than Lua's logical live-byte count. Zero disables the byte limit.

Reusable definitions, callable metadata and instance bookkeeping containers use System allocators but are **outside the per-VM byte budget**. Native object internals, callable captures and custom converter allocations follow their own allocators. Built-in System-string conversion uses System storage; explicitly using `std::string` retains that type's standard allocator. The budget is not a total application heap cap.

Allocation failures return an error and preserve existing allocations. Failed registrations may retain unpublished definitions until shutdown to keep partially created closures safe. A caller should not retry permanent failures indefinitely. Provider callbacks must not reenter the same VM.

## Instruction budgets
Default: 100,000 Lua VM instructions per `execute`/`call`; zero disables the limit. Hooks run in quanta of 100 instructions, so limits below or between quanta are approximate. Lua-defined `__gc` finalizers are rejected by the exposed `setmetatable` wrapper, because Lua disables instruction hooks while running GC finalizers. Ordinary table metatables remain supported. Native binding finalizers still destroy Lua-owned C++ objects and must remain bounded. If a script catches the instruction error, the host operation still reports `InstructionLimit`.

This is a cooperative instruction budget, not a wall-clock timeout or a hard real-time guarantee. Parsing, garbage-collector native work and bound C++ functions are not preempted by it. Long-running native operations must have their own bounded/cancellable contract. Ordinary runtime errors leave the VM usable; host side effects and globals already changed remain changed.

## Capabilities and standard libraries
Configuration selects base, table, string, math and UTF-8 libraries independently. The runtime amalgamation links the Lua core and only these libraries. No package/dynamic-loader, filesystem I/O, OS, debug or coroutine library is opened. Base `dofile`, `loadfile`, `load`, `print` and `rawset` are removed. Supply application functions explicitly for output, storage or hardware access. `execute` accepts text only, never binary chunks.

The Lua source contains other upstream library files for provenance, but they are not compiled into this runtime. Upstream portable runtime internals remain upstream code; the ESPressio binding introduces no hardware-specific operations. Supply an application-selected hash seed if deterministic default hashing is unsuitable.

The configuration constrains ordinary script capabilities. It is not a security boundary against malicious native bindings, unsafe converters or engine vulnerabilities. Custom converters receive Lua's C API for extension and must preserve all invariants; application-exposed native operations determine what scripts can ultimately do.
