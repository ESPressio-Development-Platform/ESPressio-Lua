#pragma once
#include "detail/ESPressio_LuaBinding.hpp"
namespace ESPressio::Lua {
namespace Detail {
struct Symbol { String name; Memory::SharedPtr<const Callable> call; bool function; };
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
