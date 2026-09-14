#pragma once

#if !__has_include(<ESPressio_EventTypeDescriptor.hpp>) || !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Lua Event integration requires the final ESPressio-Event and ESPressio-Primitive redesign surfaces."
#endif

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>

#include <ESPressio_EventTypeDescriptor.hpp>
#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_LuaInstance.hpp"

namespace ESPressio::Lua {

enum class LuaEventAuthorizationDecision : std::uint8_t {
    Authorized,
    Denied
};

/// <summary>
/// Application-owned authorization policy for Lua-origin Event dispatch.
/// </summary>
/// <remarks>
/// Primitive discovery and Event schema metadata are descriptive only. They never
/// authorize Lua to construct or dispatch an occurrence.
/// </remarks>
class ILuaEventAuthorizer {
public:
    virtual ~ILuaEventAuthorizer() = default;
    virtual LuaEventAuthorizationDecision Authorize(
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept = 0;
};

namespace Detail {

inline const Primitive::PrimitiveTypeDescriptor& RequireLuaEventCommonDescriptor(
    Primitive::TypeDirectoryView view,
    std::size_t luaIndex
) {
    require(view.IsFrozen(), "Primitive TypeDirectory view is unavailable");
    require(luaIndex != 0 && luaIndex <= view.Size(), "Primitive descriptor index out of range");
    const auto& common = view.begin()[luaIndex - 1];
    require(common.Key.Family == Event::EventFamilyId, "Primitive descriptor is not an Event");
    return common;
}

inline std::size_t LuaEventMaximumPayloadBytes(
    const Serializable::StaticSchemaDescriptor& schema,
    Event::EventPayloadFormat format
) noexcept {
    switch (format) {
        case Event::EventPayloadFormat::DirectBinary:
            return schema.MaximumDirectBinaryBytes;
        case Event::EventPayloadFormat::CBOR:
            return schema.MaximumCborBytes;
        case Event::EventPayloadFormat::JSON:
            return schema.MaximumJsonBytes;
    }
    return 0;
}

inline Event::EventDynamicDispatchStatus DispatchLuaEvent(
    Primitive::TypeDirectoryView view,
    const ILuaEventAuthorizer& authorizer,
    std::size_t luaIndex,
    Event::EventPayloadFormat format,
    std::string_view payload
) {
    const auto& common = RequireLuaEventCommonDescriptor(view, luaIndex);
    const auto* descriptor = Event::GetEventTypeDescriptor(common);
    require(descriptor != nullptr && descriptor->Schema != nullptr,
            "Event does not expose bounded P3 metadata");
    require(authorizer.Authorize(common) == LuaEventAuthorizationDecision::Authorized,
            "Lua Event dispatch is not authorized");
    require(descriptor->DynamicallyConstructible && descriptor->DispatchSerialized != nullptr,
            "Event is not dynamically constructible");

    const auto maximum = LuaEventMaximumPayloadBytes(*descriptor->Schema, format);
    require(maximum != 0, "Event representation is unavailable");
    require(payload.size() <= maximum, "Lua Event payload exceeds bounded P3 capacity");

    return Event::DispatchDynamicEvent(
        *descriptor,
        format,
        reinterpret_cast<const std::uint8_t*>(payload.data()),
        payload.size()).Status;
}

} // namespace Detail

/// <summary>
/// Registers bounded Lua-origin Event dispatch over one frozen Primitive directory.
/// </summary>
/// <remarks>
/// The TypeDirectoryView and authorizer are borrowed and must outlive Lua calls through
/// this module. Lua owns no Event registry, listener/target topology, occurrence pool,
/// retry path, transport, worker or ConceptualMessageId lifecycle. The Event family owns
/// P3 decode, construction and dispatch through its already-frozen Type runtime.
/// </remarks>
inline Result RegisterEventDispatch(
    Instance& instance,
    Primitive::TypeDirectoryView view,
    const ILuaEventAuthorizer& authorizer,
    std::string_view moduleName = "Event"
) noexcept {
    if (!view.IsFrozen()) {
        return Result::failure(Status::InvalidState,
                               "Primitive TypeDirectory must be frozen before Lua Event registration");
    }

    try {
        Module module(moduleName);

        module.constant("Accepted", Event::EventDynamicDispatchStatus::Accepted);
        module.constant("NotConstructible", Event::EventDynamicDispatchStatus::NotConstructible);
        module.constant("UnsupportedFormat", Event::EventDynamicDispatchStatus::UnsupportedFormat);
        module.constant("SchemaOrDecodeFailure", Event::EventDynamicDispatchStatus::SchemaOrDecodeFailure);
        module.constant("NotInitialized", Event::EventDynamicDispatchStatus::NotInitialized);
        module.constant("Stopping", Event::EventDynamicDispatchStatus::Stopping);
        module.constant("CapacityUnavailable", Event::EventDynamicDispatchStatus::CapacityUnavailable);
        module.constant("IdentityUnavailable", Event::EventDynamicDispatchStatus::IdentityUnavailable);
        module.constant("IdentifierExhausted", Event::EventDynamicDispatchStatus::IdentifierExhausted);

        module.function("dispatchDirectBinary",
            [view, &authorizer](std::size_t index, std::string_view payload) {
                return Detail::DispatchLuaEvent(
                    view, authorizer, index, Event::EventPayloadFormat::DirectBinary, payload);
            });
        module.function("dispatchCbor",
            [view, &authorizer](std::size_t index, std::string_view payload) {
                return Detail::DispatchLuaEvent(
                    view, authorizer, index, Event::EventPayloadFormat::CBOR, payload);
            });
        module.function("dispatchJson",
            [view, &authorizer](std::size_t index, std::string_view payload) {
                return Detail::DispatchLuaEvent(
                    view, authorizer, index, Event::EventPayloadFormat::JSON, payload);
            });

        return instance.registerModule(module);
    } catch (const BindingError& error) {
        return Result::failure(Status::InvalidState, error.what());
    } catch (const std::bad_alloc&) {
        return Result::failure(Status::MemoryError, "Unable to allocate Lua Event module");
    } catch (...) {
        return Result::failure(Status::RuntimeError, "Unable to register Lua Event module");
    }
}

} // namespace ESPressio::Lua
