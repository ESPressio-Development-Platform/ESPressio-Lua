#pragma once
#include "ESPressio_LuaType.hpp"
#include "ESPressio_LuaModule.hpp"
#include "../vendor/lua/lualib.h"
#ifndef ESPRESSIO_LUA_ENABLE_LOGGING
#define ESPRESSIO_LUA_ENABLE_LOGGING 0
#endif
#if ESPRESSIO_LUA_ENABLE_LOGGING
#include <ESPressio_Logger.hpp>
#endif

namespace ESPressio::Lua {
/// <summary>Outcome of a protected host operation.</summary>
enum class Status { Success, InvalidState, Busy, SyntaxError, RuntimeError, MemoryError, InstructionLimit };
/// <summary>Allocation-free operation result. Messages may be truncated to 511 bytes.</summary>
struct Result {
    Status status = Status::Success;
    char message[512]{};
    explicit operator bool() const noexcept { return status == Status::Success; }
    static Result failure(Status status, const char* message) noexcept {
        Result result; result.status = status;
        if (message) { std::strncpy(result.message, message, sizeof(result.message) - 1); }
        return result;
    }
};
/// <summary>Per-instance resource and standard-library configuration.</summary>
struct Configuration {
    /// <summary>Captured at construction; the provider must outlive the scripting instance.</summary>
    Memory::IMemoryProvider* memoryProvider = nullptr;
    Memory::MemoryPolicy memoryPolicy = Memory::MemoryPolicy::Automatic;
    /// <summary>Actual Lua heap and binding-owned native storage limit; zero is unlimited.</summary>
    std::size_t memoryLimitBytes = 256 * 1024;
    /// <summary>Lua instructions per execute/call operation; zero disables the cooperative budget.</summary>
    std::size_t instructionLimit = 100000;
    /// <summary>Hash seed supplied to Lua 5.5. Inject a platform entropy-derived seed when appropriate.</summary>
    unsigned int hashSeed = 0x45535052u;
    bool baseLibrary = true;
    bool tableLibrary = true;
    bool stringLibrary = true;
    bool mathLibrary = true;
    bool utf8Library = true;
};

/// <summary>Owns an independent Lua VM and its registered capabilities.</summary>
/// <remarks>Concurrent/reentrant operations return Busy. Destruction requires external quiescence.
/// Borrowed objects, callable captures and providers must remain alive. No raw lua_State is exposed.</remarks>
class Instance final {
    struct Registration { Memory::SharedPtr<Detail::TypeData> type; bool installed = false; };
    struct ModuleRegistration { Memory::SharedPtr<Detail::ModuleData> module; bool installed = false; };
    Memory::Vector<ModuleRegistration> modules_;
    Configuration configuration_;
    Detail::MemoryAccount memory_;
    lua_State* state_ = nullptr;
    Memory::Vector<Registration> types_;
    Memory::Vector<Memory::SharedPtr<const Detail::Callable>> functions_;
    std::atomic_flag active_ = ATOMIC_FLAG_INIT;
    std::size_t instructionsRemaining_ = 0;
    bool instructionExceeded_ = false;
    Result initialization_;
    inline static char exportsKey_;

    static Instance& self(lua_State* state) noexcept { return **static_cast<Instance**>(lua_getextraspace(state)); }
    static void instructionHook(lua_State* state, lua_Debug*) {
        auto& instance = self(state);
        constexpr std::size_t quantum = 100;
        if (instance.instructionsRemaining_ <= quantum) {
            instance.instructionExceeded_ = true;
            luaL_error(state, "Lua instruction budget exceeded");
            return;
        }
        instance.instructionsRemaining_ -= quantum;
    }
    static int globalWrite(lua_State* state) {
        lua_rawgetp(state, LUA_REGISTRYINDEX, &exportsKey_);
        lua_pushvalue(state, 2); lua_rawget(state, -2);
        if (!lua_isnil(state, -1)) return luaL_error(state, "Registered globals are read-only");
        lua_pop(state, 2);
        lua_rawset(state, 1);
        return 0;
    }
    static int setTableMetatable(lua_State* state) {
        // Lua deliberately disables instruction hooks inside GC finalizers. Allow
        // ordinary table metatables, but never let scripts register a __gc handler.
        if (lua_istable(state, 2)) {
            lua_pushliteral(state, "__gc"); lua_rawget(state, 2);
            const bool finalizer = !lua_isnil(state, -1); lua_pop(state, 1);
            if (finalizer) return luaL_error(state, "Lua-defined __gc finalizers are disabled");
        }
        lua_pushvalue(state, lua_upvalueindex(1));
        lua_insert(state, 1);
        lua_call(state, lua_gettop(state) - 1, 1);
        return 1;
    }
    static int functionDispatch(lua_State* state) {
        return Detail::guarded(state, [&] {
            auto* call = static_cast<const Detail::Callable*>(lua_touserdata(state, lua_upvalueindex(1)));
            return call->invoke(state, nullptr, 1);
        });
    }
    static int moduleIndex(lua_State* state) {
        return Detail::guarded(state, [&] {
            auto* module = static_cast<const Detail::ModuleData*>(lua_touserdata(state, lua_upvalueindex(1)));
            const auto name = Converter<std::string_view>::read(state, 2);
            for (const auto& symbol : module->symbols) {
                if (std::string_view(symbol.name.data(), symbol.name.size()) != name) continue;
                if (!symbol.function) return symbol.call->invoke(state, nullptr, 1);
                lua_pushlightuserdata(state, const_cast<Detail::Callable*>(symbol.call.get()));
                lua_pushcclosure(state, functionDispatch, 1);
                return 1;
            }
            throw BindingError("Module member is not exposed");
        });
    }
    template<class F> static int operation(lua_State* state) {
        auto& call = *static_cast<F*>(lua_touserdata(state, 1));
        lua_settop(state, 0);
        return Detail::guarded(state, [&] { call(state); return 0; });
    }
    template<class F> Result run(F&& function, bool budget = false) {
        if (!state_) return Result::failure(Status::InvalidState, "Lua instance is not initialized");
        if (active_.test_and_set(std::memory_order_acquire)) return Result::failure(Status::Busy, "Lua instance is already executing");
        struct Exit {
            Instance& owner; int top;
            ~Exit() { lua_sethook(owner.state_, nullptr, 0, 0); lua_settop(owner.state_, top); owner.active_.clear(std::memory_order_release); }
        } exit{*this, lua_gettop(state_)};
        instructionExceeded_ = false;
        if (budget && configuration_.instructionLimit) {
            instructionsRemaining_ = configuration_.instructionLimit;
            lua_sethook(state_, instructionHook, LUA_MASKCOUNT, 100);
        }
        // Neither a zero-upvalue C function nor a lightuserdata push allocates.
        // All allocating API calls execute within the protected trampoline.
        if (!lua_checkstack(state_, 2)) return Result::failure(Status::MemoryError, "Cannot reserve Lua stack");
        lua_pushcfunction(state_, &operation<std::remove_reference_t<F>>);
        lua_pushlightuserdata(state_, &function);
        int code = lua_pcall(state_, 1, 0, 0);
        if (code == LUA_OK && !instructionExceeded_) return {};
        const Status status = instructionExceeded_ ? Status::InstructionLimit : code == LUA_ERRMEM ? Status::MemoryError : code == LUA_ERRSYNTAX ? Status::SyntaxError : Status::RuntimeError;
        const char* message = lua_type(state_, -1) == LUA_TSTRING ? lua_tostring(state_, -1) : "Lua failed with a non-string error or non-standard native exception";
        auto result = Result::failure(status, message);
#if ESPRESSIO_LUA_ENABLE_LOGGING
        ESPRESSIO_LOG_ERROR(Logging::LogCategory::Named("ESPressio.Lua"), result.message);
#endif
        return result;
    }
    void pushExports(lua_State* state) { lua_rawgetp(state, LUA_REGISTRYINDEX, &exportsKey_); }
    void ensureName(lua_State* state, std::string_view name) {
        Detail::nameCheck(name);
        pushExports(state);
        lua_pushlstring(state, name.data(), name.size()); lua_rawget(state, -2);
        bool available = lua_isnil(state, -1); lua_pop(state, 2);
        lua_pushglobaltable(state);
        lua_pushlstring(state, name.data(), name.size()); lua_rawget(state, -2);
        available = available && lua_isnil(state, -1); lua_pop(state, 2);
        Detail::require(available, "Global name is already in use");
    }
    // Value is at the top of the stack. The final rawset is the publication point.
    void publish(lua_State* state, std::string_view name) {
        pushExports(state);
        lua_pushlstring(state, name.data(), name.size());
        lua_pushvalue(state, -3);
        lua_rawset(state, -3);
        lua_pop(state, 2);
    }
    bool registered(const Detail::TypeData* type) const noexcept {
        for (const auto& entry : types_) if (entry.type.get() == type && entry.installed) return true;
        return false;
    }
    void initializeLibraries(lua_State* state) {
        const struct { const char* name; lua_CFunction open; bool enabled; } libraries[] = {
            {LUA_GNAME, luaopen_base, configuration_.baseLibrary},
            {LUA_TABLIBNAME, luaopen_table, configuration_.tableLibrary},
            {LUA_STRLIBNAME, luaopen_string, configuration_.stringLibrary},
            {LUA_MATHLIBNAME, luaopen_math, configuration_.mathLibrary},
            {LUA_UTF8LIBNAME, luaopen_utf8, configuration_.utf8Library}
        };
        for (const auto& library : libraries) if (library.enabled) {
            luaL_requiref(state, library.name, library.open, 1); lua_pop(state, 1);
        }
        if (configuration_.baseLibrary) {
            lua_getglobal(state, "setmetatable");
            lua_pushcclosure(state, setTableMetatable, 1);
            lua_setglobal(state, "setmetatable");
        }
        // Host capabilities are explicit. No filesystem, OS, loader, debug or coroutine library.
        for (const char* name : {"dofile", "loadfile", "load", "print", "rawset"}) {
            lua_pushnil(state); lua_setglobal(state, name);
        }
        lua_newtable(state); lua_rawsetp(state, LUA_REGISTRYINDEX, &exportsKey_);
        lua_pushglobaltable(state); lua_newtable(state);
        pushExports(state); lua_setfield(state, -2, "__index");
        lua_pushcfunction(state, globalWrite); lua_setfield(state, -2, "__newindex");
        lua_pushboolean(state, true); lua_setfield(state, -2, "__metatable");
        lua_setmetatable(state, -2); lua_pop(state, 1);
    }
public:
    /// <summary>Creates a VM. Inspect initializationResult before registering or executing.</summary>
    explicit Instance(Configuration configuration = {})
        : configuration_(configuration), memory_{configuration.memoryProvider ? configuration.memoryProvider : &Memory::GetProvider(), configuration.memoryPolicy, configuration.memoryLimitBytes} {
        static_assert(LUA_VERSION_RELEASE_NUM == 50501, "ESPressio-Lua requires the bundled Lua 5.5.1 runtime");
        if (!memory_.provider->Supports(memory_.policy)) {
            initialization_ = Result::failure(Status::MemoryError, "Memory provider does not support the requested policy"); return;
        }
        state_ = lua_newstate(&Detail::MemoryAccount::reallocate, &memory_, configuration_.hashSeed);
        if (!state_) { initialization_ = Result::failure(Status::MemoryError, "Cannot allocate Lua state"); return; }
        *static_cast<Instance**>(lua_getextraspace(state_)) = this;
        initialization_ = run([&](lua_State* state) { initializeLibraries(state); });
        if (!initialization_) { lua_close(state_); state_ = nullptr; }
    }
    /// <summary>Finalizes Lua-owned objects before releasing type/callable definitions.</summary>
    ~Instance() { if (state_) lua_close(state_); }
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;
    /// <summary>Gets VM initialization outcome, including allocation failures.</summary>
    const Result& initializationResult() const noexcept { return initialization_; }
    /// <summary>Gets reserved Lua/native bytes. Read only while the instance is quiescent.</summary>
    std::size_t memoryUsed() const noexcept { return memory_.used; }
    /// <summary>Gets peak reserved bytes, including temporary reallocation overlap.</summary>
    std::size_t peakMemoryUsed() const noexcept { return memory_.peak; }

    /// <summary>Freezes and installs a reusable definition. Repeating the same registration is idempotent.</summary>
    template<class T> Result registerType(Type<T>& type) {
        return run([&](lua_State* state) {
            if (registered(type.data_.get())) return;
            ensureName(state, type.name());
            type.freeze();
            // Retain even after a partial allocation failure: any Lua closure must remain safe.
            types_.push_back({type.data_, false});
            auto& entry = types_.back();
            auto* data = type.data_.get();
            lua_newtable(state);
            lua_pushlightuserdata(state, data); lua_pushcclosure(state, Detail::indexObject, 1); lua_setfield(state, -2, "__index");
            lua_pushlightuserdata(state, data); lua_pushcclosure(state, Detail::writeObject, 1); lua_setfield(state, -2, "__newindex");
            lua_pushcfunction(state, Detail::destroyObject); lua_setfield(state, -2, "__gc");
            lua_pushboolean(state, true); lua_setfield(state, -2, "__metatable");
            lua_pushlstring(state, type.name().data(), type.name().size()); lua_setfield(state, -2, "__name");
            lua_rawsetp(state, LUA_REGISTRYINDEX, data);
            // A userdata type object cannot be modified with table/raw operations.
            lua_newuserdatauv(state, 1, 0); lua_newtable(state);
            lua_pushlightuserdata(state, data); lua_pushlightuserdata(state, &memory_);
            lua_pushcclosure(state, Detail::constructObject, 2); lua_setfield(state, -2, "__call");
            lua_pushboolean(state, true); lua_setfield(state, -2, "__metatable");
            lua_setmetatable(state, -2);
            publish(state, type.name());
            entry.installed = true;
        });
    }
    /// <summary>Freezes and installs a reusable namespace of functions and constants.</summary>
    Result registerModule(Module& module) {
        return run([&](lua_State* state) {
            for (const auto& entry : modules_) if (entry.module == module.data_ && entry.installed) return;
            ensureName(state, module.name());
            module.freeze();
            modules_.push_back({module.data_, false});
            lua_newuserdatauv(state, 1, 0); lua_newtable(state);
            lua_pushlightuserdata(state, module.data_.get()); lua_pushcclosure(state, moduleIndex, 1);
            lua_setfield(state, -2, "__index");
            lua_pushboolean(state, true); lua_setfield(state, -2, "__metatable");
            lua_setmetatable(state, -2);
            publish(state, module.name());
            modules_.back().installed = true;
        });
    }
    /// <summary>Exposes an application-owned native object through an already registered definition.</summary>
    /// <remarks>The object must outlive this Instance, including its finalizers. Lua never deletes it.</remarks>
    template<class T> Result registerInstance(std::string_view name, const Type<T>& type, T& object, BorrowedOwnership = Borrowed) {
        return run([&](lua_State* state) {
            Detail::require(registered(type.data_.get()), "Register the Lua type before exposing its instances");
            ensureName(state, name);
            auto& wrapper = Detail::newObject(state, type.data_.get(), &memory_);
            wrapper.pointer = &object;
            publish(state, name);
        });
    }
    /// <summary>Registers a free function or typed callable. Captures must remain valid for the VM lifetime.</summary>
    template<class F> Result registerFunction(std::string_view name, F function) {
        return run([&](lua_State* state) {
            ensureName(state, name);
            auto call = Detail::function(std::move(function));
            functions_.push_back(call);
            lua_pushlightuserdata(state, const_cast<Detail::Callable*>(call.get()));
            lua_pushcclosure(state, functionDispatch, 1);
            publish(state, name);
        });
    }
    /// <summary>Registers a copied, read-only global value. Enum values use their underlying integer.</summary>
    template<class V> Result registerConstant(std::string_view name, const V& value) {
        return run([&](lua_State* state) {
            ensureName(state, name);
            Converter<std::decay_t<decltype(value)>>::push(state, value);
            Detail::require(!lua_isnil(state, -1), "A named constant cannot be nil");
            publish(state, name);
        });
    }
    /// <summary>Compiles and executes a text-only chunk with a fresh instruction budget.</summary>
    Result execute(std::string_view source, const char* chunkName = "script") {
        int loadCode = LUA_OK;
        auto result = run([&](lua_State* state) {
            loadCode = luaL_loadbufferx(state, source.data(), source.size(), chunkName ? chunkName : "script", "t");
            if (loadCode != LUA_OK) { lua_error(state); return; }
            lua_call(state, 0, 0);
        }, true);
        if (!result && loadCode == LUA_ERRSYNTAX) result.status = Status::SyntaxError;
        return result;
    }
    /// <summary>Calls a Lua global function and discards its return values.</summary>
    template<class... A> Result call(std::string_view name, const A&... arguments) {
        return run([&](lua_State* state) {
            lua_pushglobaltable(state); lua_pushlstring(state, name.data(), name.size()); lua_gettable(state, -2); lua_remove(state, -2);
            Detail::require(lua_isfunction(state, -1), "Global is not a function");
            Detail::require(lua_checkstack(state, sizeof...(A) + 2), "Cannot reserve call argument stack");
            (Converter<std::decay_t<decltype(arguments)>>::push(state, arguments), ...);
            lua_call(state, sizeof...(A), 0);
        }, true);
    }
    /// <summary>Reads a Lua global through its checked value converter. Output changes only after conversion succeeds.</summary>
    template<class V> Result readGlobal(std::string_view name, V& output) {
        return run([&](lua_State* state) {
            lua_pushglobaltable(state); lua_pushlstring(state, name.data(), name.size()); lua_gettable(state, -2);
            auto converted = Converter<V>::read(state, -1);
            output = std::move(converted);
        });
    }
    /// <summary>Runs a full collection; unreachable Lua-owned native objects are finalized.</summary>
    Result collectGarbage() { return run([](lua_State* state) { lua_gc(state, LUA_GCCOLLECT); }, true); }
};
} // namespace ESPressio::Lua
