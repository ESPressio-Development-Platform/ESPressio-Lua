#pragma once

#if !__has_include(<ESPressio_StateDescriptor.hpp>) || !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Lua State integration requires the final ESPressio-State and ESPressio-Primitive redesign surfaces."
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>

#include <ESPressio_StateDescriptor.hpp>
#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_LuaInstance.hpp"

namespace ESPressio::Lua {

enum class LuaStateAuthorizationDecision : std::uint8_t {
    Authorized,
    Denied
};

/// <summary>Application-owned authorization policy for Lua-origin State inspection.</summary>
/// <remarks>
/// Primitive and State descriptors are discovery metadata only. Descriptor visibility never
/// grants permission to inspect current State truth.
/// </remarks>
class ILuaStateAuthorizer {
public:
    virtual ~ILuaStateAuthorizer() = default;
    virtual LuaStateAuthorizationDecision Authorize(
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept = 0;
};

template<std::size_t TMaximumPayloadBytes>
struct LuaStateReadResult final {
    State::StateDynamicReadStatus Status = State::StateDynamicReadStatus::SerializationFailure;
    std::array<std::uint8_t, TMaximumPayloadBytes> Payload{};
    std::size_t Bytes = 0;
    Timing::QualifiedTime TruthTime{};
};

template<std::size_t TMaximumPayloadBytes>
struct Converter<LuaStateReadResult<TMaximumPayloadBytes>> {
    static LuaStateReadResult<TMaximumPayloadBytes> read(lua_State*, int) = delete;

    static void push(lua_State* state, const LuaStateReadResult<TMaximumPayloadBytes>& value) {
        lua_createtable(state, 0, 5);

        lua_pushinteger(state, static_cast<lua_Integer>(value.Status));
        lua_setfield(state, -2, "status");

        if (value.Status == State::StateDynamicReadStatus::Success) {
            lua_pushlstring(
                state,
                reinterpret_cast<const char*>(value.Payload.data()),
                value.Bytes);
            lua_setfield(state, -2, "payload");

            const auto nanoseconds = value.TruthTime.Nanoseconds;
            lua_pushinteger(state, static_cast<lua_Integer>(nanoseconds >> 32U));
            lua_setfield(state, -2, "truthNanosecondsHigh32");
            lua_pushinteger(state, static_cast<lua_Integer>(nanoseconds & 0xffffffffULL));
            lua_setfield(state, -2, "truthNanosecondsLow32");
            lua_pushinteger(state, static_cast<lua_Integer>(value.TruthTime.Reliability));
            lua_setfield(state, -2, "truthReliability");
        } else {
            lua_pushnil(state);
            lua_setfield(state, -2, "payload");
            lua_pushnil(state);
            lua_setfield(state, -2, "truthNanosecondsHigh32");
            lua_pushnil(state);
            lua_setfield(state, -2, "truthNanosecondsLow32");
            lua_pushnil(state);
            lua_setfield(state, -2, "truthReliability");
        }
    }
};

namespace Detail {

inline const Primitive::PrimitiveTypeDescriptor& RequireLuaStateCommonDescriptor(
    Primitive::TypeDirectoryView view,
    std::size_t luaIndex
) {
    require(view.IsFrozen(), "Primitive TypeDirectory view is unavailable");
    require(luaIndex != 0 && luaIndex <= view.Size(), "Primitive descriptor index out of range");
    const auto& common = view.begin()[luaIndex - 1];
    require(common.Key.Family == State::StateFamilyId, "Primitive descriptor is not State");
    return common;
}

inline std::size_t LuaStateMaximumPayloadBytes(
    const State::StateTypeDescriptor& descriptor,
    State::StatePayloadFormat format
) noexcept {
    const auto raw = static_cast<std::uint8_t>(format);
    if (raw < 1 || raw > 3) return 0;
    return descriptor.MaximumSerializedValueBytes[raw - 1];
}

template<std::size_t TMaximumPayloadBytes>
LuaStateReadResult<TMaximumPayloadBytes> ReadLuaState(
    Primitive::TypeDirectoryView view,
    const ILuaStateAuthorizer& authorizer,
    std::size_t luaIndex,
    State::StatePayloadFormat format
) {
    static_assert(TMaximumPayloadBytes > 0, "Lua State inspection requires finite nonzero output capacity");

    const auto& common = RequireLuaStateCommonDescriptor(view, luaIndex);
    const auto* descriptor = State::GetStateTypeDescriptor(common);
    require(descriptor != nullptr && descriptor->ValueSchema != nullptr,
            "State does not expose bounded read-only P3 metadata");
    require(authorizer.Authorize(common) == LuaStateAuthorizationDecision::Authorized,
            "Lua State inspection is not authorized");
    require(LuaStateMaximumPayloadBytes(*descriptor, format) != 0,
            "State representation is unavailable");

    LuaStateReadResult<TMaximumPayloadBytes> value{};
    const auto result = State::ReadDynamicState(
        *descriptor,
        format,
        value.Payload.data(),
        value.Payload.size());
    value.Status = result.Status;
    value.Bytes = result.Bytes;
    value.TruthTime = result.TruthTime;
    require(value.Bytes <= value.Payload.size(), "State read exceeded configured Lua capacity");
    return value;
}

} // namespace Detail

/// <summary>Registers bounded read-only State inspection over one frozen Primitive directory.</summary>
/// <remarks>
/// The TypeDirectoryView and authorizer are borrowed and must outlive Lua calls through the
/// module. Each read uses one compile-time fixed output buffer, and the returned Lua table
/// contains the family-owned status plus an atomic payload/TruthTime snapshot on success.
/// Lua owns no State storage, writer capability, convergence/session control, retry path,
/// transport, routing or worker. Local/non-Serializable State fails explicitly.
/// </remarks>
template<std::size_t TMaximumPayloadBytes>
Result RegisterStateInspection(
    Instance& instance,
    Primitive::TypeDirectoryView view,
    const ILuaStateAuthorizer& authorizer,
    std::string_view moduleName = "State"
) noexcept {
    static_assert(TMaximumPayloadBytes > 0, "Lua State inspection requires finite nonzero output capacity");
    if (!view.IsFrozen()) {
        return Result::failure(Status::InvalidState,
                               "Primitive TypeDirectory must be frozen before Lua State registration");
    }

    try {
        Module module(moduleName);

        module.constant("Success", State::StateDynamicReadStatus::Success);
        module.constant("NoValue", State::StateDynamicReadStatus::NoValue);
        module.constant("InsufficientOutput", State::StateDynamicReadStatus::InsufficientOutput);
        module.constant("SerializationFailure", State::StateDynamicReadStatus::SerializationFailure);
        module.constant("UnsupportedFormat", State::StateDynamicReadStatus::UnsupportedFormat);

        module.function("readDirectBinary",
            [view, &authorizer](std::size_t index) {
                return Detail::ReadLuaState<TMaximumPayloadBytes>(
                    view, authorizer, index, State::StatePayloadFormat::DirectBinary);
            });
        module.function("readCbor",
            [view, &authorizer](std::size_t index) {
                return Detail::ReadLuaState<TMaximumPayloadBytes>(
                    view, authorizer, index, State::StatePayloadFormat::CBOR);
            });
        module.function("readJson",
            [view, &authorizer](std::size_t index) {
                return Detail::ReadLuaState<TMaximumPayloadBytes>(
                    view, authorizer, index, State::StatePayloadFormat::JSON);
            });

        return instance.registerModule(module);
    } catch (const BindingError& error) {
        return Result::failure(Status::InvalidState, error.what());
    } catch (const std::bad_alloc&) {
        return Result::failure(Status::MemoryError, "Unable to allocate Lua State module");
    } catch (...) {
        return Result::failure(Status::RuntimeError, "Unable to register Lua State module");
    }
}

} // namespace ESPressio::Lua
