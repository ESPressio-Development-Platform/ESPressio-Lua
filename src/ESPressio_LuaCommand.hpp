#pragma once

#if !__has_include(<ESPressio_CommandDescriptor.hpp>) || !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Lua Command integration requires the final ESPressio-Command and ESPressio-Primitive redesign surfaces."
#endif

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>

#include <ESPressio_CommandDescriptor.hpp>
#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_LuaInstance.hpp"

namespace ESPressio::Lua {

enum class LuaCommandAuthorizationDecision : std::uint8_t {
    Authorized,
    Denied
};

/// <summary>
/// Application-owned authorization policy for Lua-origin Command submission.
/// </summary>
/// <remarks>
/// Primitive TypeDirectory discovery is metadata only. Registering Command tooling never
/// converts descriptor visibility into permission to execute a Command.
/// </remarks>
class ILuaCommandAuthorizer {
public:
    virtual ~ILuaCommandAuthorizer() = default;
    virtual LuaCommandAuthorizationDecision Authorize(
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept = 0;
};

namespace Detail {

inline const Primitive::PrimitiveTypeDescriptor& RequireLuaCommandCommonDescriptor(
    Primitive::TypeDirectoryView view,
    std::size_t luaIndex
) {
    require(view.IsFrozen(), "Primitive TypeDirectory view is unavailable");
    require(luaIndex != 0 && luaIndex <= view.Size(), "Primitive descriptor index out of range");
    const auto& common = view.begin()[luaIndex - 1];
    require(common.Key.Family == Command::CommandFamilyId, "Primitive descriptor is not a Command");
    return common;
}

inline std::size_t LuaCommandMaximumPayloadBytes(
    const Command::CommandTypeDescriptor& descriptor,
    Command::CommandPayloadFormat format
) noexcept {
    if (descriptor.RequestSchema == nullptr) return 0;
    switch (format) {
        case Command::CommandPayloadFormat::DirectBinary:
            return descriptor.RequestSchema->MaximumDirectBinaryBytes;
        case Command::CommandPayloadFormat::CBOR:
            return descriptor.RequestSchema->MaximumCborBytes;
        case Command::CommandPayloadFormat::JSON:
            return descriptor.RequestSchema->MaximumJsonBytes;
    }
    return 0;
}

inline Command::CommandSubmissionStatus SubmitLuaCommand(
    Primitive::TypeDirectoryView view,
    const ILuaCommandAuthorizer& authorizer,
    std::size_t luaIndex,
    Command::CommandPayloadFormat format,
    std::string_view payload
) {
    const auto& common = RequireLuaCommandCommonDescriptor(view, luaIndex);
    const auto* descriptor = Command::GetCommandTypeDescriptor(common);
    require(descriptor != nullptr && descriptor->RequestSchema != nullptr,
            "Command does not expose bounded P3 request metadata");
    require(authorizer.Authorize(common) == LuaCommandAuthorizationDecision::Authorized,
            "Lua Command submission is not authorized");

    if (descriptor->DynamicConstruction == Command::CommandDynamicConstructionMode::RequesterRequired) {
        throw BindingError("Response-bearing Command requires requester capability");
    }
    require(descriptor->DynamicConstruction == Command::CommandDynamicConstructionMode::FireAndForget &&
            descriptor->SubmitSerialized != nullptr,
            "Command does not support dynamic fire-and-forget construction");

    const auto maximum = LuaCommandMaximumPayloadBytes(*descriptor, format);
    require(maximum != 0, "Command representation is unavailable");
    require(payload.size() <= maximum, "Lua Command payload exceeds bounded P3 capacity");

    return Command::SubmitDynamicCommand(
        *descriptor,
        format,
        reinterpret_cast<const std::uint8_t*>(payload.data()),
        payload.size()).Status;
}

} // namespace Detail

/// <summary>
/// Registers bounded Lua-origin typed Command submission over one frozen Primitive directory.
/// </summary>
/// <remarks>
/// The TypeDirectoryView and authorizer must outlive every Lua call through the module.
/// The adapter owns no Command registry, request pool, requester capability, retry queue,
/// response sink, transport, or execution worker. Command owns typed construction and admission.
/// Response-bearing Commands are rejected explicitly rather than silently downgraded to
/// fire-and-forget semantics. Input strings are borrowed only for the duration of each call.
/// </remarks>
inline Result RegisterCommandAdmission(
    Instance& instance,
    Primitive::TypeDirectoryView view,
    const ILuaCommandAuthorizer& authorizer,
    std::string_view moduleName = "Command"
) noexcept {
    if (!view.IsFrozen()) {
        return Result::failure(Status::InvalidState,
                               "Primitive TypeDirectory must be frozen before Lua Command registration");
    }

    try {
        Module module(moduleName);

        module.constant("Accepted", Command::CommandSubmissionStatus::Accepted);
        module.constant("CapacityUnavailable", Command::CommandSubmissionStatus::CapacityUnavailable);
        module.constant("SchemaOrDecodeFailure", Command::CommandSubmissionStatus::SchemaOrDecodeFailure);
        module.constant("ServiceUnavailable", Command::CommandSubmissionStatus::NotInitialized);

        module.function("submitDirectBinary",
            [view, &authorizer](std::size_t index, std::string_view payload) {
                return Detail::SubmitLuaCommand(
                    view, authorizer, index, Command::CommandPayloadFormat::DirectBinary, payload);
            });
        module.function("submitCbor",
            [view, &authorizer](std::size_t index, std::string_view payload) {
                return Detail::SubmitLuaCommand(
                    view, authorizer, index, Command::CommandPayloadFormat::CBOR, payload);
            });
        module.function("submitJson",
            [view, &authorizer](std::size_t index, std::string_view payload) {
                return Detail::SubmitLuaCommand(
                    view, authorizer, index, Command::CommandPayloadFormat::JSON, payload);
            });

        return instance.registerModule(module);
    } catch (const BindingError& error) {
        return Result::failure(Status::InvalidState, error.what());
    } catch (const std::bad_alloc&) {
        return Result::failure(Status::MemoryError, "Unable to allocate Lua Command module");
    } catch (...) {
        return Result::failure(Status::RuntimeError, "Unable to register Lua Command module");
    }
}

} // namespace ESPressio::Lua
