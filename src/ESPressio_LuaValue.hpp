#pragma once
#include <ESPressio_Memory.hpp>
// Use the C++ limits header: some embedded C headers hide LLONG_MAX in C++ mode.
#include <climits>
#include "../vendor/lua/lua.h"
#include "../vendor/lua/lauxlib.h"
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace ESPressio::Lua {
namespace Memory = ESPressio::System::Memory;

/// <summary>Reports invalid host registration or value conversion without allocating a message.</summary>
class BindingError : public std::exception {
    const char* message_;
public:
    explicit BindingError(const char* message) noexcept : message_(message) {}
    const char* what() const noexcept override { return message_; }
};

namespace Detail {
template<class> inline constexpr bool Unsupported = false;
inline void require(bool condition, const char* message) {
    if (!condition) throw BindingError(message);
}
inline void nameCheck(std::string_view name) {
    require(!name.empty() && name.find('\0') == std::string_view::npos,
            "Binding names must be nonempty and contain no NUL bytes");
}
}

/// <summary>Specialize for additional value types. read must validate; push leaves exactly one Lua value.</summary>
/// <remarks>Raw native pointers/references have no implicit conversion or automatic exposure.</remarks>
template<class T, class Enable = void> struct Converter {
    static T read(lua_State*, int) {
        static_assert(Detail::Unsupported<T>, "Provide ESPressio::Lua::Converter<T> for this value type");
    }
    static void push(lua_State*, const T&) {
        static_assert(Detail::Unsupported<T>, "Provide ESPressio::Lua::Converter<T> or explicitly expose the native object");
    }
};

/// <summary>Strict boolean conversion; numbers and strings are not silently coerced.</summary>
template<> struct Converter<bool> {
    static bool read(lua_State* state, int index) {
        Detail::require(lua_type(state, index) == LUA_TBOOLEAN, "Expected boolean");
        return lua_toboolean(state, index) != 0;
    }
    static void push(lua_State* state, bool value) { lua_pushboolean(state, value); }
};

/// <summary>Checked integral conversion, preserving Lua's signed integer range.</summary>
template<class T> struct Converter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
    static T read(lua_State* state, int index) {
        Detail::require(lua_isinteger(state, index), "Expected integer");
        const auto value = lua_tointeger(state, index);
        if constexpr (std::is_signed_v<T>) {
            if constexpr (sizeof(T) < sizeof(lua_Integer))
                Detail::require(value >= std::numeric_limits<T>::min() && value <= std::numeric_limits<T>::max(), "Integer out of range");
        } else {
            Detail::require(value >= 0, "Expected nonnegative integer");
            if constexpr (sizeof(T) < sizeof(lua_Unsigned))
                Detail::require(static_cast<lua_Unsigned>(value) <= std::numeric_limits<T>::max(), "Integer out of range");
        }
        return static_cast<T>(value);
    }
    static void push(lua_State* state, T value) {
        if constexpr (std::is_unsigned_v<T>) {
            if constexpr (sizeof(T) >= sizeof(lua_Integer))
                Detail::require(value <= static_cast<T>(std::numeric_limits<lua_Integer>::max()), "Unsigned value exceeds Lua integer range");
        } else if constexpr (sizeof(T) > sizeof(lua_Integer)) {
            Detail::require(value >= std::numeric_limits<lua_Integer>::min() && value <= std::numeric_limits<lua_Integer>::max(), "Integer exceeds Lua range");
        }
        lua_pushinteger(state, static_cast<lua_Integer>(value));
    }
};

/// <summary>Finite numeric conversion with overflow checks; numeric strings are rejected.</summary>
template<class T> struct Converter<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static T read(lua_State* state, int index) {
        Detail::require(lua_type(state, index) == LUA_TNUMBER, "Expected number");
        const auto value = lua_tonumber(state, index);
        Detail::require(std::isfinite(value) && value >= std::numeric_limits<T>::lowest() && value <= std::numeric_limits<T>::max(), "Number out of range");
        return static_cast<T>(value);
    }
    static void push(lua_State* state, T value) {
        Detail::require(std::isfinite(value) && value >= std::numeric_limits<lua_Number>::lowest() && value <= std::numeric_limits<lua_Number>::max(), "Number exceeds Lua range");
        lua_pushnumber(state, static_cast<lua_Number>(value));
    }
};

/// <summary>Enum values use their checked underlying integer representation; register named constants separately.</summary>
template<class T> struct Converter<T, std::enable_if_t<std::is_enum_v<T>>> {
    using Underlying = std::underlying_type_t<T>;
    static T read(lua_State* state, int index) { return static_cast<T>(Converter<Underlying>::read(state, index)); }
    static void push(lua_State* state, T value) { Converter<Underlying>::push(state, static_cast<Underlying>(value)); }
};

/// <summary>Binary-safe owning string conversion, including ESPressio allocator-backed strings.</summary>
template<class Traits, class Allocator> struct Converter<std::basic_string<char, Traits, Allocator>> {
    using String = std::basic_string<char, Traits, Allocator>;
    static String read(lua_State* state, int index) {
        Detail::require(lua_type(state, index) == LUA_TSTRING, "Expected string");
        std::size_t size = 0;
        const char* data = lua_tolstring(state, index, &size);
        return String(data, size);
    }
    static void push(lua_State* state, const String& value) { lua_pushlstring(state, value.data(), value.size()); }
};

/// <summary>Non-owning string input remains valid only during the native call. Lua copies output strings.</summary>
template<> struct Converter<std::string_view> {
    static std::string_view read(lua_State* state, int index) {
        Detail::require(lua_type(state, index) == LUA_TSTRING, "Expected string");
        std::size_t size = 0;
        const char* data = lua_tolstring(state, index, &size);
        return {data, size};
    }
    static void push(lua_State* state, std::string_view value) { lua_pushlstring(state, value.data(), value.size()); }
};

/// <summary>C strings are copied on output; nil maps to null. Inputs must not be retained beyond the call.</summary>
template<> struct Converter<const char*> {
    static const char* read(lua_State* state, int index) {
        if (lua_isnil(state, index)) return nullptr;
        auto value = Converter<std::string_view>::read(state, index);
        Detail::require(value.find('\0') == std::string_view::npos, "C string contains embedded NUL");
        return value.data();
    }
    static void push(lua_State* state, const char* value) { if (value) lua_pushstring(state, value); else lua_pushnil(state); }
};

/// <summary>Missing optional values are represented by nil.</summary>
template<class T> struct Converter<std::optional<T>> {
    static std::optional<T> read(lua_State* state, int index) {
        if (lua_isnil(state, index)) return std::nullopt;
        return Converter<T>::read(state, index);
    }
    static void push(lua_State* state, const std::optional<T>& value) {
        if (value) Converter<T>::push(state, *value); else lua_pushnil(state);
    }
};
} // namespace ESPressio::Lua
