#pragma once
#include "../ESPressio_LuaValue.hpp"
#include <algorithm>
#include <atomic>
#include <functional>
#include <tuple>
#include <utility>

namespace ESPressio::Lua::Detail {
using String = Memory::String<>;

/// <summary>Accounts actual Lua and native-object storage against a captured provider and byte budget.</summary>
struct MemoryAccount {
    Memory::IMemoryProvider* provider;
    Memory::MemoryPolicy policy;
    std::size_t limit;
    std::size_t used = 0;
    std::size_t peak = 0;

    void* allocate(std::size_t bytes, std::size_t alignment) {
        if (bytes > std::numeric_limits<std::size_t>::max() - used || (limit && bytes > limit - std::min(limit, used)))
            throw std::bad_alloc();
        void* block = provider->Allocate(bytes, alignment, policy);
        if (!block) throw std::bad_alloc();
        used += bytes;
        peak = std::max(peak, used);
        return block;
    }
    void release(void* block, std::size_t bytes, std::size_t alignment) noexcept {
        if (!block) return;
        provider->Deallocate(block, bytes, alignment, policy);
        used -= bytes;
    }
    struct alignas(std::max_align_t) Header { std::size_t capacity; };
    static void* reallocate(void* context, void* pointer, std::size_t oldSize, std::size_t newSize) noexcept {
        auto& account = *static_cast<MemoryAccount*>(context);
        auto* oldHeader = pointer ? static_cast<Header*>(pointer) - 1 : nullptr;
        if (!newSize) {
            if (oldHeader) account.release(oldHeader, sizeof(Header) + oldHeader->capacity, alignof(Header));
            return nullptr;
        }
        // Lua requires shrinking to succeed. Keep capacity and account actual reserved bytes.
        if (oldHeader && newSize <= oldHeader->capacity) return pointer;
        if (newSize > std::numeric_limits<std::size_t>::max() - sizeof(Header)) return nullptr;
        try {
            auto* header = static_cast<Header*>(account.allocate(sizeof(Header) + newSize, alignof(Header)));
            ::new (header) Header{newSize};
            if (pointer) {
                std::memcpy(header + 1, pointer, std::min(oldSize, newSize));
                account.release(oldHeader, sizeof(Header) + oldHeader->capacity, alignof(Header));
            }
            return header + 1;
        } catch (...) { return nullptr; } // Allocator callbacks must never throw into Lua.
    }
};

struct TypeData;
/// <summary>Native object wrapper. Only the owning form destroys/releases the native allocation.</summary>
struct Object {
    void* pointer = nullptr;
    const TypeData* type = nullptr;
    MemoryAccount* account = nullptr;
    void (*destroy)(Object&) noexcept = nullptr;
};

/// <summary>Type-erased native operation allocated through ESPressio System.</summary>
struct Callable {
    virtual ~Callable() = default;
    virtual int invoke(lua_State*, Object*, int) const = 0;
};
template<class F> struct CallableModel final : Callable {
    F function;
    explicit CallableModel(F value) : function(std::move(value)) {}
    int invoke(lua_State* state, Object* object, int start) const override { return function(state, object, start); }
};
template<class F> Memory::SharedPtr<const Callable> callable(F function) {
    return Memory::MakeShared<CallableModel<F>>(std::move(function));
}
struct Member {
    String name;
    Memory::SharedPtr<const Callable> method;
    Memory::SharedPtr<const Callable> getter;
    Memory::SharedPtr<const Callable> setter;
};
struct Constructor { int arity; Memory::SharedPtr<const Callable> call; };
struct TypeData {
    String name;
    bool frozen = false;
    Memory::Vector<Member> members;
    Memory::Vector<Constructor> constructors;
    explicit TypeData(std::string_view value) : name(value.data(), value.size()) { nameCheck(value); }
    void mutableCheck() const { require(!frozen, "Lua type definition is frozen"); }
    void memberCheck(std::string_view value) const {
        mutableCheck(); nameCheck(value);
        require(value.substr(0, 2) != "__", "Reserved Lua metamethod name");
        for (const auto& member : members)
            require(std::string_view(member.name.data(), member.name.size()) != value, "Duplicate member name");
    }
};

inline Object& objectAt(lua_State* state, int index, const TypeData* expected) {
    require(lua_type(state, index) == LUA_TUSERDATA && lua_rawlen(state, index) == sizeof(Object), "Expected native object");
    require(lua_getmetatable(state, index), "Native object has no metatable");
    lua_rawgetp(state, LUA_REGISTRYINDEX, expected);
    const bool matches = lua_rawequal(state, -1, -2);
    lua_pop(state, 2);
    require(matches, "Native object belongs to a different Lua type definition");
    auto& object = *static_cast<Object*>(lua_touserdata(state, index));
    require(object.type == expected && object.pointer, "Native object is no longer valid");
    return object;
}
inline int destroyObject(lua_State* state) noexcept {
    auto* object = static_cast<Object*>(lua_touserdata(state, 1));
    if (object && object->pointer && object->destroy) object->destroy(*object);
    if (object) object->pointer = nullptr;
    return 0;
}
inline Object& newObject(lua_State* state, const TypeData* type, MemoryAccount* account) {
    auto* object = static_cast<Object*>(lua_newuserdatauv(state, sizeof(Object), 0));
    ::new (object) Object{nullptr, type, account, nullptr};
    lua_rawgetp(state, LUA_REGISTRYINDEX, type);
    require(lua_istable(state, -1), "Lua type is not registered in this instance");
    lua_setmetatable(state, -2); // Finalizer is installed before native construction can throw.
    return *object;
}

template<class T> void destroyNative(Object& object) noexcept {
    static_assert(std::is_nothrow_destructible_v<T>, "Lua-owned native destructors must be noexcept");
    static_cast<T*>(object.pointer)->~T();
    object.account->release(object.pointer, sizeof(T), alignof(T));
}

template<class A> auto argument(lua_State* state, int index) {
    static_assert(!std::is_lvalue_reference_v<A> || std::is_const_v<std::remove_reference_t<A>>,
                  "Mutable reference arguments require an explicit adapter; converted values are copies");
    return Converter<std::decay_t<A>>::read(state, index);
}
template<class Tuple, std::size_t... I> auto arguments(lua_State* state, int start, std::index_sequence<I...>) {
    (void)state; (void)start;
    // List initialization validates left-to-right before entering application code.
    return std::tuple<std::decay_t<std::tuple_element_t<I, Tuple>>...>{argument<std::tuple_element_t<I, Tuple>>(state, start + static_cast<int>(I))...};
}
template<class F> struct Traits : Traits<decltype(&F::operator())> {};
template<class R, class... A> struct Traits<R(*)(A...)> { using Return = R; using Args = std::tuple<A...>; };
template<class R, class... A> struct Traits<R(*)(A...) noexcept> : Traits<R(*)(A...)> {};
template<class C, class R, class... A> struct Traits<R(C::*)(A...)> : Traits<R(*)(A...)> {};
template<class C, class R, class... A> struct Traits<R(C::*)(A...) const> : Traits<R(*)(A...)> {};
template<class C, class R, class... A> struct Traits<R(C::*)(A...) noexcept> : Traits<R(*)(A...)> {};
template<class C, class R, class... A> struct Traits<R(C::*)(A...) const noexcept> : Traits<R(*)(A...)> {};

template<class R, class F> int result(lua_State* state, F&& call) {
    if constexpr (std::is_void_v<R>) { call(); return 0; }
    else { decltype(auto) value = call(); Converter<std::decay_t<R>>::push(state, value); return 1; }
}
template<class T, class F> auto method(F function) {
    using Args = typename Traits<F>::Args;
    using Return = typename Traits<F>::Return;
    return callable([function](lua_State* state, Object* object, int start) {
        constexpr auto count = std::tuple_size_v<Args>;
        require(lua_gettop(state) - start + 1 == static_cast<int>(count), "Wrong number of native arguments");
        auto values = arguments<Args>(state, start, std::make_index_sequence<count>{});
        return result<Return>(state, [&]() -> decltype(auto) {
            return std::apply([&](auto&&... args) -> decltype(auto) {
                return std::invoke(function, *static_cast<T*>(object->pointer), std::forward<decltype(args)>(args)...);
            }, std::move(values));
        });
    });
}
template<class F> auto function(F functionValue) {
    using Args = typename Traits<F>::Args;
    using Return = typename Traits<F>::Return;
    return callable([functionValue](lua_State* state, Object*, int start) {
        constexpr auto count = std::tuple_size_v<Args>;
        require(lua_gettop(state) - start + 1 == static_cast<int>(count), "Wrong number of native arguments");
        auto values = arguments<Args>(state, start, std::make_index_sequence<count>{});
        return result<Return>(state, [&]() -> decltype(auto) { return std::apply(functionValue, std::move(values)); });
    });
}

/// <summary>Only catches standard C++ exceptions; Lua's private unwind exception must propagate unchanged.</summary>
template<class F> int guarded(lua_State* state, F&& call) {
    try { return call(); }
    catch (const std::exception& error) { return luaL_error(state, "%s", error.what()); }
}
inline int dispatchMethod(lua_State* state) {
    return guarded(state, [&] {
        auto* type = static_cast<const TypeData*>(lua_touserdata(state, lua_upvalueindex(1)));
        auto* call = static_cast<const Callable*>(lua_touserdata(state, lua_upvalueindex(2)));
        return call->invoke(state, &objectAt(state, 1, type), 2);
    });
}
inline void pushMethod(lua_State* state, const TypeData* type, const Callable* call) {
    lua_pushlightuserdata(state, const_cast<TypeData*>(type));
    lua_pushlightuserdata(state, const_cast<Callable*>(call));
    lua_pushcclosure(state, dispatchMethod, 2);
}
inline int indexObject(lua_State* state) {
    return guarded(state, [&] {
        auto* type = static_cast<const TypeData*>(lua_touserdata(state, lua_upvalueindex(1)));
        auto& object = objectAt(state, 1, type);
        const auto key = Converter<std::string_view>::read(state, 2);
        for (const auto& member : type->members) {
            if (std::string_view(member.name.data(), member.name.size()) != key) continue;
            if (member.method) { pushMethod(state, type, member.method.get()); return 1; }
            lua_settop(state, 1);
            return member.getter->invoke(state, &object, 2);
        }
        throw BindingError("Native member is not exposed");
    });
}
inline int writeObject(lua_State* state) {
    return guarded(state, [&] {
        auto* type = static_cast<const TypeData*>(lua_touserdata(state, lua_upvalueindex(1)));
        auto& object = objectAt(state, 1, type);
        const auto key = Converter<std::string_view>::read(state, 2);
        for (const auto& member : type->members) {
            if (std::string_view(member.name.data(), member.name.size()) != key) continue;
            require(bool(member.setter), "Native member is read-only");
            lua_remove(state, 2);
            return member.setter->invoke(state, &object, 2);
        }
        throw BindingError("Native member is not exposed");
    });
}
inline int constructObject(lua_State* state) {
    return guarded(state, [&] {
        auto* type = static_cast<const TypeData*>(lua_touserdata(state, lua_upvalueindex(1)));
        auto* account = static_cast<MemoryAccount*>(lua_touserdata(state, lua_upvalueindex(2)));
        const int arity = lua_gettop(state) - 1;
        for (const auto& constructor : type->constructors) {
            if (constructor.arity != arity) continue;
            auto& object = newObject(state, type, account);
            return constructor.call->invoke(state, &object, 2);
        }
        throw BindingError("No exposed constructor accepts this number of arguments");
    });
}
} // namespace ESPressio::Lua::Detail
