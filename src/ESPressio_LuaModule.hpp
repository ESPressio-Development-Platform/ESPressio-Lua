#pragma once
#include "detail/ESPressio_LuaBinding.hpp"
namespace ESPressio::Lua {
namespace Detail {
/**
 * ESPressio Memory Audit
 * Members:
 * - name (String): 12 bytes [Capacity + 1 bytes backing buffer when allocated]
 * - call (Memory::SharedPtr<Callable>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * - function (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 24 bytes [name: Capacity + 1 bytes backing buffer when allocated; call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
struct Symbol { String name; Memory::SharedPtr<const Callable> call; bool function; };
/**
 * ESPressio Memory Audit
 * Members:
 * - name (String): 12 bytes [Capacity + 1 bytes backing buffer when allocated]
 * - frozen (bool): 1 bytes [0 bytes dynamic allocation]
 * - symbols (Memory::Vector<Symbol>): 16 bytes [Capacity * (24 bytes) element storage; N live elements each: name: Capacity + 1 bytes backing buffer when allocated; N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Total Memory: 32 bytes [name: Capacity + 1 bytes backing buffer when allocated; symbols: Capacity * (24 bytes) element storage; symbols: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; symbols: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
struct ModuleData {
    String name;
    bool frozen = false;
    Memory::Vector<Symbol> symbols;
    explicit ModuleData(std::string_view value) : name(value.data(), value.size()) { nameCheck(value); }
    void check(std::string_view value) const {
        require(!frozen, "Lua module definition is frozen"); nameCheck(value);
        for (const auto& symbol : symbols)
            require(std::string_view(symbol.name.data(), symbol.name.size()) != value, "Duplicate module member");
    }
};
}
/// <summary>Reusable read-only Lua namespace containing native functions and copied constants.</summary>
/// <remarks>Copies share identity; first registration freezes the definition. Register enum members as constants.</remarks>
/**
 * ESPressio Memory Audit
 * Members:
 * - data_ (Memory::SharedPtr<Detail::ModuleData>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 32 bytes; pointee: name: Capacity + 1 bytes backing buffer when allocated; pointee: symbols: Capacity * (24 bytes) element storage; pointee: symbols: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; pointee: symbols: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Total Memory: 8 bytes [data_: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 32 bytes; data_: pointee: name: Capacity + 1 bytes backing buffer when allocated; data_: pointee: symbols: Capacity * (24 bytes) element storage; data_: pointee: symbols: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; data_: pointee: symbols: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class Module {
    Memory::SharedPtr<Detail::ModuleData> data_;
    friend class Instance;
public:
    explicit Module(std::string_view name) : data_(Memory::MakeShared<Detail::ModuleData>(name)) {}
    /// <summary>Gets the namespace's global name.</summary>
    std::string_view name() const noexcept { return {data_->name.data(), data_->name.size()}; }
    /// <summary>Freezes the shared namespace definition before sharing it between host contexts.</summary>
    Module& freeze() noexcept { data_->frozen = true; return *this; }
    /// <summary>Exposes a typed free function or callable as module.function(...).</summary>
    template<class F> Module& function(std::string_view name, F function) {
        data_->check(name);
        data_->symbols.push_back({Detail::String(name.data(), name.size()), Detail::function(std::move(function)), true});
        return *this;
    }
    /// <summary>Copies a constant into the reusable definition. String inputs are copied with the System allocator.</summary>
    template<class V> Module& constant(std::string_view name, const V& value) {
        data_->check(name);
        Memory::SharedPtr<const Detail::Callable> getter;
        if constexpr (std::is_convertible_v<const V&, std::string_view>) {
            if constexpr (std::is_pointer_v<std::decay_t<V>>)
                Detail::require(value != nullptr, "Module string constants cannot be null");
            const std::string_view view(value);
            Detail::String copy(view.data(), view.size());
            getter = Detail::callable([copy = std::move(copy)](lua_State* state, Detail::Object*, int) {
                Converter<Detail::String>::push(state, copy); return 1;
            });
        } else {
            getter = Detail::callable([copy = value](lua_State* state, Detail::Object*, int) {
                Converter<std::decay_t<decltype(copy)>>::push(state, copy); return 1;
            });
        }
        data_->symbols.push_back({Detail::String(name.data(), name.size()), std::move(getter), false});
        return *this;
    }
};
} // namespace ESPressio::Lua
