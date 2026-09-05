#pragma once
#include "detail/ESPressio_LuaBinding.hpp"

namespace ESPressio::Lua {
class Instance;
/// <summary>Reusable Lua-facing definition of an unmodified native C++ type.</summary>
/// <remarks>Copies share identity. First registration freezes every copy. Instances retain definition storage.</remarks>
template<class T> class Type {
    Memory::SharedPtr<Detail::TypeData> data_;
    friend class Instance;
public:
    /// <summary>Creates a mutable definition. Separate definitions can expose different views of T.</summary>
    explicit Type(std::string_view name) : data_(Memory::MakeShared<Detail::TypeData>(name)) {}
    /// <summary>Gets the Lua-facing name.</summary>
    std::string_view name() const noexcept { return {data_->name.data(), data_->name.size()}; }
    /// <summary>Indicates whether registration has frozen the shared definition.</summary>
    bool frozen() const noexcept { return data_->frozen; }
    /// <summary>Explicitly freezes this definition before sharing it between host execution contexts.</summary>
    Type& freeze() noexcept { data_->frozen = true; return *this; }
    /// <summary>Exposes a native constructor. Overloads must have distinct argument counts.</summary>
    template<class... A> Type& constructor() {
        data_->mutableCheck();
        for (const auto& entry : data_->constructors)
            Detail::require(entry.arity != sizeof...(A), "Constructor overloads must have distinct arities");
        auto call = Detail::callable([](lua_State* state, Detail::Object* object, int start) {
            auto values = Detail::arguments<std::tuple<A...>>(state, start, std::index_sequence_for<A...>{});
            void* storage = object->account->allocate(sizeof(T), alignof(T));
            try {
                std::apply([&](auto&&... args) { ::new (storage) T(std::forward<decltype(args)>(args)...); }, std::move(values));
            } catch (...) {
                object->account->release(storage, sizeof(T), alignof(T));
                throw;
            }
            object->pointer = storage;
            object->destroy = &Detail::destroyNative<T>;
            return 1; // The owning userdata is already at the top of the stack.
        });
        data_->constructors.push_back({static_cast<int>(sizeof...(A)), std::move(call)});
        return *this;
    }
    /// <summary>Exposes a member function; overloads require an explicit member-pointer cast.</summary>
    template<class F> Type& method(std::string_view name, F function) {
        static_assert(std::is_member_function_pointer_v<F>, "method expects a member function pointer");
        data_->memberCheck(name);
        data_->members.push_back({Detail::String(name.data(), name.size()), Detail::method<T>(function), {}, {}});
        return *this;
    }
    /// <summary>Exposes a read/write data member, or a read-only field when its native type is const.</summary>
    template<class V> Type& field(std::string_view name, V T::* field) {
        data_->memberCheck(name);
        auto getter = Detail::callable([field](lua_State* state, Detail::Object* object, int) {
            Converter<std::remove_cv_t<V>>::push(state, static_cast<T*>(object->pointer)->*field); return 1;
        });
        Memory::SharedPtr<const Detail::Callable> setter;
        if constexpr (!std::is_const_v<V>) setter = Detail::callable([field](lua_State* state, Detail::Object* object, int start) {
            auto value = Converter<V>::read(state, start);
            static_cast<T*>(object->pointer)->*field = std::move(value); return 0;
        });
        data_->members.push_back({Detail::String(name.data(), name.size()), {}, std::move(getter), std::move(setter)});
        return *this;
    }
    /// <summary>Exposes a data member without allowing script assignment.</summary>
    template<class V> Type& readOnlyField(std::string_view name, V T::* field) {
        data_->memberCheck(name);
        auto getter = Detail::callable([field](lua_State* state, Detail::Object* object, int) {
            Converter<std::remove_cv_t<V>>::push(state, static_cast<T*>(object->pointer)->*field); return 1;
        });
        data_->members.push_back({Detail::String(name.data(), name.size()), {}, std::move(getter), {}});
        return *this;
    }
    /// <summary>Exposes a computed property through a zero-argument getter and one-argument setter.</summary>
    template<class G, class S> Type& property(std::string_view name, G getter, S setter) {
        static_assert(std::tuple_size_v<typename Detail::Traits<G>::Args> == 0, "Property getter must take no arguments");
        static_assert(std::tuple_size_v<typename Detail::Traits<S>::Args> == 1, "Property setter must take one argument");
        static_assert(!std::is_void_v<typename Detail::Traits<G>::Return>, "Property getter must return a value");
        static_assert(std::is_void_v<typename Detail::Traits<S>::Return>, "Property setter must return void");
        data_->memberCheck(name);
        data_->members.push_back({Detail::String(name.data(), name.size()), {}, Detail::method<T>(getter), Detail::method<T>(setter)});
        return *this;
    }
    /// <summary>Exposes a computed property through a zero-argument getter.</summary>
    template<class G> Type& readOnlyProperty(std::string_view name, G getter) {
        static_assert(std::tuple_size_v<typename Detail::Traits<G>::Args> == 0, "Property getter must take no arguments");
        static_assert(!std::is_void_v<typename Detail::Traits<G>::Return>, "Property getter must return a value");
        data_->memberCheck(name);
        data_->members.push_back({Detail::String(name.data(), name.size()), {}, Detail::method<T>(getter), {}});
        return *this;
    }
};
/// <summary>Explicit non-owning registration. The native object must outlive the scripting instance.</summary>
struct BorrowedOwnership {};
inline constexpr BorrowedOwnership Borrowed{};
} // namespace ESPressio::Lua
