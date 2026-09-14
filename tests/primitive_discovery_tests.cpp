#include <ESPressio_LuaPrimitiveDiscovery.hpp>

#include <cassert>
#include <cstdint>

using namespace ESPressio;

namespace {

Primitive::PrimitiveTypeDescriptor CommandDescriptor() noexcept {
    Primitive::PrimitiveTypeDescriptor descriptor{};
    descriptor.Key = {Primitive::FamilyIds::Command, 0x0000000000000011ULL};
    descriptor.CanonicalName = "Command.Alpha";
    descriptor.Capabilities = Primitive::PrimitiveTypeCapabilities{
        static_cast<std::uint8_t>(Primitive::PrimitiveTypeCapability::Serializable)
    };
    descriptor.Versions = {1, 1};
    descriptor.SerializedSize.MaximumCompletePrimitiveWireBytes = 64;
    descriptor.FamilyExtension.Data = reinterpret_cast<const void*>(0x1234);
    return descriptor;
}

Primitive::PrimitiveTypeDescriptor EventDescriptor() noexcept {
    Primitive::ContractFingerprint::Storage fingerprint{};
    fingerprint[0] = 0xA5;
    fingerprint[31] = 0x5A;

    Primitive::PrimitiveTypeDescriptor descriptor{};
    descriptor.Key = {Primitive::FamilyIds::Event, 0xFEDCBA9876543210ULL};
    descriptor.CanonicalName = "Event.BigUnsignedId";
    descriptor.Capabilities = Primitive::PrimitiveTypeCapabilities{
        static_cast<std::uint8_t>(Primitive::PrimitiveTypeCapability::Serializable) |
        static_cast<std::uint8_t>(Primitive::PrimitiveTypeCapability::Transmissible)
    };
    descriptor.Versions = {2, 7};
    descriptor.Contract = Primitive::ContractFingerprint{fingerprint};
    descriptor.SerializedSize.MaximumCompletePrimitiveWireBytes = 512;
    descriptor.FamilyExtension.Data = reinterpret_cast<const void*>(0x5678);
    return descriptor;
}

} // namespace

int main() {
    Lua::Instance unavailableInstance;
    assert(unavailableInstance.initializationResult());
    Primitive::TypeDirectoryView unavailable;
    const auto unavailableResult = Lua::RegisterPrimitiveDiscovery(unavailableInstance, unavailable);
    assert(unavailableResult.status == Lua::Status::InvalidState);

    Primitive::TypeDirectory<4> directory;
    // Register in the opposite order to the final family/key ordering.
    assert(directory.Register(EventDescriptor()) == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Register(CommandDescriptor()) == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);

    const auto view = directory.View();
    assert(view.IsFrozen());
    assert(view.Size() == 2);

    Lua::Instance instance;
    assert(instance.initializationResult());
    const auto registration = Lua::RegisterPrimitiveDiscovery(instance, view);
    assert(registration);

    const auto script = instance.execute(R"LUA(
        assert(Primitive.count() == 2)

        -- TypeDirectory order is deterministic, not registration order.
        assert(Primitive.family(1) == 2)
        assert(Primitive.name(1) == "Command.Alpha")
        assert(Primitive.typeIdHigh32(1) == 0)
        assert(Primitive.typeIdLow32(1) == 0x11)
        assert(Primitive.serializable(1) == true)
        assert(Primitive.transmissible(1) == false)
        assert(Primitive.protocolMinimum(1) == 1)
        assert(Primitive.protocolMaximum(1) == 1)
        assert(Primitive.maximumWireBytes(1) == 64)
        assert(Primitive.contractIsZero(1) == true)

        assert(Primitive.family(2) == 3)
        assert(Primitive.name(2) == "Event.BigUnsignedId")
        -- Exact uint64 Type identity survives Lua's signed integer boundary.
        assert(Primitive.typeIdHigh32(2) == 0xFEDCBA98)
        assert(Primitive.typeIdLow32(2) == 0x76543210)
        assert(Primitive.serializable(2) == true)
        assert(Primitive.transmissible(2) == true)
        assert(Primitive.protocolMinimum(2) == 2)
        assert(Primitive.protocolMaximum(2) == 7)
        assert(Primitive.maximumWireBytes(2) == 512)
        assert(Primitive.contractIsZero(2) == false)
        assert(Primitive.contractByte(2, 1) == 0xA5)
        assert(Primitive.contractByte(2, 32) == 0x5A)

        assert(Primitive.findByName(3, "Event.BigUnsignedId") == 2)
        assert(Primitive.findByName(3, "event.bigunsignedid") == nil)
        assert(Primitive.findByKey(3, 0xFEDCBA98, 0x76543210) == 2)
        assert(Primitive.findByKey(3, 0xFEDCBA98, 0x76543211) == nil)

        -- Opaque family-extension behavior is not a Lua discovery capability.
        local extensionVisible = pcall(function() return Primitive.familyExtension end)
        assert(extensionVisible == false)
    )LUA", "primitive-discovery-contract");
    assert(script);

    // The Lua module is a live read-only view over the frozen directory, not a second registry.
    assert(view.Find({Primitive::FamilyIds::Event, 0xFEDCBA9876543210ULL}) != nullptr);
    return 0;
}
