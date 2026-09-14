#pragma once

#if !__has_include(<ESPressio_TypeDirectory.hpp>)
#error "ESPressio Lua Primitive discovery requires ESPressio-Primitive TypeDirectory support."
#endif

#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <string_view>

#include <ESPressio_TypeDirectory.hpp>

#include "ESPressio_LuaInstance.hpp"

namespace ESPressio::Lua {
namespace Detail {

inline const Primitive::PrimitiveTypeDescriptor& RequirePrimitiveDescriptor(
    Primitive::TypeDirectoryView view,
    std::size_t luaIndex
) {
    require(view.IsFrozen(), "Primitive TypeDirectory view is unavailable");
    require(luaIndex != 0 && luaIndex <= view.Size(), "Primitive descriptor index out of range");
    return view.begin()[luaIndex - 1];
}

inline std::optional<std::size_t> PrimitiveDescriptorIndex(
    Primitive::TypeDirectoryView view,
    const Primitive::PrimitiveTypeDescriptor* descriptor
) noexcept {
    if (!view.IsFrozen() || descriptor == nullptr || view.begin() == nullptr) return std::nullopt;
    return static_cast<std::size_t>(descriptor - view.begin()) + 1;
}

inline std::uint64_t PrimitiveTypeValue(std::uint32_t high, std::uint32_t low) noexcept {
    return (static_cast<std::uint64_t>(high) << 32U) | static_cast<std::uint64_t>(low);
}

} // namespace Detail

/// <summary>
/// Registers a read-only Lua module backed directly by one frozen Primitive
/// TypeDirectoryView. No descriptor or family extension registry is copied into Lua.
/// </summary>
/// <remarks>
/// The owner of the TypeDirectoryView must outlive every Lua call through the
/// registered module. Discovery is metadata only and never grants construction,
/// dispatch, admission, State mutation, routing or transport authority.
///
/// Descriptor indexes are one-based for Lua. Type IDs are exposed and accepted as
/// exact high/low 32-bit halves because Lua integers are signed and cannot represent
/// every valid uint64_t Primitive Type ID. FamilyExtension is intentionally not
/// exposed: family-owned opaque behavior is not a discovery capability.
/// </remarks>
inline Result RegisterPrimitiveDiscovery(
    Instance& instance,
    Primitive::TypeDirectoryView view,
    std::string_view moduleName = "Primitive"
) noexcept {
    if (!view.IsFrozen()) {
        return Result::failure(Status::InvalidState, "Primitive TypeDirectory must be frozen before Lua discovery registration");
    }

    try {
        Module module(moduleName);

        module.function("count", [view]() noexcept -> std::size_t {
            return view.Size();
        });

        module.function("family", [view](std::size_t index) -> Primitive::PrimitiveFamilyId {
            return Detail::RequirePrimitiveDescriptor(view, index).Key.Family;
        });

        module.function("typeIdHigh32", [view](std::size_t index) -> std::uint32_t {
            return static_cast<std::uint32_t>(
                Detail::RequirePrimitiveDescriptor(view, index).Key.TypeValue >> 32U
            );
        });

        module.function("typeIdLow32", [view](std::size_t index) -> std::uint32_t {
            return static_cast<std::uint32_t>(
                Detail::RequirePrimitiveDescriptor(view, index).Key.TypeValue & 0xffffffffULL
            );
        });

        module.function("name", [view](std::size_t index) -> std::string_view {
            return Detail::RequirePrimitiveDescriptor(view, index).CanonicalName;
        });

        module.function("serializable", [view](std::size_t index) -> bool {
            return Detail::RequirePrimitiveDescriptor(view, index).Capabilities.Contains(
                Primitive::PrimitiveTypeCapability::Serializable
            );
        });

        module.function("transmissible", [view](std::size_t index) -> bool {
            return Detail::RequirePrimitiveDescriptor(view, index).Capabilities.Contains(
                Primitive::PrimitiveTypeCapability::Transmissible
            );
        });

        module.function("protocolMinimum", [view](std::size_t index) -> Primitive::PrimitiveProtocolVersion {
            return Detail::RequirePrimitiveDescriptor(view, index).Versions.Minimum;
        });

        module.function("protocolMaximum", [view](std::size_t index) -> Primitive::PrimitiveProtocolVersion {
            return Detail::RequirePrimitiveDescriptor(view, index).Versions.Maximum;
        });

        module.function("maximumWireBytes", [view](std::size_t index) -> std::size_t {
            return Detail::RequirePrimitiveDescriptor(view, index).SerializedSize.MaximumCompletePrimitiveWireBytes;
        });

        module.function("contractIsZero", [view](std::size_t index) -> bool {
            return Detail::RequirePrimitiveDescriptor(view, index).Contract.IsZero();
        });

        module.function("contractByte", [view](std::size_t index, std::size_t byteIndex) -> std::uint8_t {
            Detail::require(
                byteIndex != 0 && byteIndex <= Primitive::ContractFingerprint::Size,
                "Primitive contract byte index out of range"
            );
            return Detail::RequirePrimitiveDescriptor(view, index).Contract.Bytes()[byteIndex - 1];
        });

        module.function(
            "findByName",
            [view](Primitive::PrimitiveFamilyId family, std::string_view name) -> std::optional<std::size_t> {
                return Detail::PrimitiveDescriptorIndex(view, view.Find(family, name));
            }
        );

        module.function(
            "findByKey",
            [view](
                Primitive::PrimitiveFamilyId family,
                std::uint32_t typeIdHigh32,
                std::uint32_t typeIdLow32
            ) -> std::optional<std::size_t> {
                const Primitive::PrimitiveTypeKey key{
                    family,
                    Detail::PrimitiveTypeValue(typeIdHigh32, typeIdLow32)
                };
                return Detail::PrimitiveDescriptorIndex(view, view.Find(key));
            }
        );

        return instance.registerModule(module);
    } catch (const BindingError& error) {
        return Result::failure(Status::InvalidState, error.what());
    } catch (const std::bad_alloc&) {
        return Result::failure(Status::MemoryError, "Unable to allocate Lua Primitive discovery module");
    } catch (...) {
        return Result::failure(Status::RuntimeError, "Unable to register Lua Primitive discovery module");
    }
}

} // namespace ESPressio::Lua
