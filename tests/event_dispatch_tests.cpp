#include <ESPressio_LuaEvent.hpp>

#include <ESPressio_Event.hpp>
#include <ESPressio_EventOutboundBinding.hpp>
#include <ESPressio_RuntimeIdentity.hpp>
#include <ESPressio_SerializationMacros.hpp>
#include <ESPressio_TransmissibleEvent.hpp>
#include <HostRuntime.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <string>
#include <thread>

using namespace ESPressio;
namespace E = ESPressio::Event;

namespace {

struct DeliveryPolicy final {
    using PolicyCategory = Primitive::OccurrenceDeliveryPolicyTag;
    using RequiredEvidence = Primitive::DestinationPrimitiveAdmission;
    using TerminalDisposition = Primitive::DiagnosticOnlyAfterBudget;
    static constexpr std::uint64_t MaximumResidenceNanoseconds = 1000000000ULL;
    static constexpr std::uint64_t MaximumAdapterAdmissionWaitNanoseconds = 1000000ULL;
    static constexpr std::uint16_t MaximumAttempts = 2;
    static constexpr std::uint64_t MinimumRetrySpacingNanoseconds = 1000ULL;
    static constexpr std::uint64_t MaximumRetrySpacingNanoseconds = 10000ULL;
};

struct ScriptEvent final : E::TransmissibleEvent<ScriptEvent> {
    static constexpr E::EventTypeId TypeId{0x8201};
    static constexpr const char* CanonicalName = "Test.Lua.ScriptEvent";
    static constexpr std::size_t MaximumLiveInstances = 2;
    static constexpr std::size_t MaximumPendingInstances = 1;
    using DeliveryPolicy = ::DeliveryPolicy;

    std::int32_t Value = 0;

    ESPRESSIO_SERIALIZABLE_TYPE(ScriptEvent)
    ESPRESSIO_SERIALIZABLE_SCHEMA_VERSION(1)
    ESPRESSIO_SERIALIZABLE_PROPERTIES(ESPRESSIO_PROPERTY("value", Value))
};

struct LocalOnlyEvent final : E::Event<LocalOnlyEvent> {
    static constexpr E::EventTypeId TypeId{0x8202};
    static constexpr const char* CanonicalName = "Test.Lua.LocalOnlyEvent";
    static constexpr std::size_t MaximumLiveInstances = 1;
    static constexpr std::size_t MaximumPendingInstances = 1;
};

class Authorizer final : public Lua::ILuaEventAuthorizer {
public:
    bool Allowed = true;
    mutable std::uint32_t Decisions = 0;

    Lua::LuaEventAuthorizationDecision Authorize(
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept override {
        ++Decisions;
        assert(descriptor.Key.Family == E::EventFamilyId);
        return Allowed
            ? Lua::LuaEventAuthorizationDecision::Authorized
            : Lua::LuaEventAuthorizationDecision::Denied;
    }
};

struct Target final {
    E::EventOutboundBinding<ScriptEvent> Binding;
    std::atomic<bool> Block{false};
    std::atomic<bool> BlockObserved{false};
    std::atomic<std::int32_t> Seen{0};
    std::atomic<std::uint32_t> Calls{0};

    bool Validate() noexcept { return true; }

    E::EventTargetAdmission Admit(const E::EventLease& lease) noexcept {
        if (Block.load(std::memory_order_acquire)) {
            BlockObserved.store(true, std::memory_order_release);
            return E::EventTargetAdmission::CapacityUnavailable;
        }
        Seen.store(lease.Get<ScriptEvent>().Value, std::memory_order_release);
        Calls.fetch_add(1, std::memory_order_acq_rel);
        return E::EventTargetAdmission::Accepted;
    }
};

template<class Predicate>
void Eventually(Predicate&& predicate) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) {
        assert(std::chrono::steady_clock::now() < limit);
        std::this_thread::yield();
    }
}

void RequireLuaSuccess(const Lua::Result& result) {
    if (!result) std::fprintf(stderr, "Lua Event contract failure: %s\n", result.message);
    assert(result);
}

std::string ProviderJsonDispatch(std::int32_t value, std::string_view assertion) {
    ScriptEvent event;
    event.Value = value;
    std::array<
        std::uint8_t,
        Serializable::MaximumSerializedSize<ScriptEvent, Serializable::JSON>> encoded{};
    const auto serialized = Serializable::SerializeBoundedJson(
        event, encoded.data(), encoded.size());
    assert(serialized);

    std::string script = "local status = Event.dispatchJson(1, [=[";
    script.append(reinterpret_cast<const char*>(encoded.data()), serialized.Bytes);
    script += "]=])\nassert(status == ";
    script.append(assertion.data(), assertion.size());
    script += ", \"status=\" .. tostring(status))";
    return script;
}

System::DeviceRuntimeIdentity Identity() {
    System::DeviceIdentifier::Storage bytes{};
    bytes[0] = 0x61;
    return {System::DeviceIdentifier{bytes}, System::RuntimeIncarnationId{0x92}};
}

} // namespace

int main() {
    HostRuntime platform;
    assert(System::RuntimeIdentity::Install(Identity()) ==
           System::RuntimeIdentity::InstallationStatus::Success);

    Primitive::TypeDirectory<2> directory;
    assert(directory.Register<LocalOnlyEvent>() == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Register<ScriptEvent>() == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);
    assert(directory.View().begin()[0].Key.TypeValue == ScriptEvent::TypeId.Value());
    assert(directory.View().begin()[1].Key.TypeValue == LocalOnlyEvent::TypeId.Value());

    E::RuntimeConfiguration configuration{};
    configuration.DispatchLane.Name = "luaEventLane";
    configuration.DispatchLane.StackSize = 4096;
    E::Runtime runtime(configuration);
    assert(runtime.Initialize(directory.View()) == E::EventRuntimeStatus::Success);

    Target target;
    assert((target.Binding.Initialize<Target, &Target::Admit, &Target::Validate>(target) ==
            E::EventRuntimeStatus::Success));
    assert(runtime.Start() == E::EventRuntimeStatus::Success);

    Authorizer authorizer;

    Lua::Instance unavailableInstance;
    assert(unavailableInstance.initializationResult());
    assert(Lua::RegisterEventDispatch(
               unavailableInstance, Primitive::TypeDirectoryView{}, authorizer).status ==
           Lua::Status::InvalidState);

    Lua::Instance instance;
    assert(instance.initializationResult());
    assert(Lua::RegisterEventDispatch(instance, directory.View(), authorizer));

    auto script = ProviderJsonDispatch(41, "Event.Accepted");
    auto result = instance.execute(script, "lua-event-json");
    RequireLuaSuccess(result);
    Eventually([&] {
        return target.Seen.load(std::memory_order_acquire) == 41 &&
               target.Calls.load(std::memory_order_acquire) == 1;
    });

    result = instance.execute(R"LUA(
        assert(Event.dispatchJson(1, [[not-json]]) == Event.SchemaOrDecodeFailure)
    )LUA", "lua-event-invalid-schema");
    RequireLuaSuccess(result);
    assert(target.Calls.load(std::memory_order_acquire) == 1);

    authorizer.Allowed = false;
    result = instance.execute(R"LUA(
        local ok = pcall(function()
            Event.dispatchJson(1, [[]])
        end)
        assert(ok == false)
    )LUA", "lua-event-authorization");
    RequireLuaSuccess(result);
    assert(target.Calls.load(std::memory_order_acquire) == 1);

    authorizer.Allowed = true;
    result = instance.execute(R"LUA(
        local localOnlyOk = pcall(function()
            Event.dispatchJson(2, [[]])
        end)
        assert(localOnlyOk == false)

        local indexOk = pcall(function()
            Event.dispatchJson(3, [[]])
        end)
        assert(indexOk == false)

        local oversizedOk = pcall(function()
            Event.dispatchJson(1, string.rep("x", 4096))
        end)
        assert(oversizedOk == false)
    )LUA", "lua-event-boundaries");
    RequireLuaSuccess(result);
    assert(target.Calls.load(std::memory_order_acquire) == 1);

    target.Block.store(true, std::memory_order_release);
    script = ProviderJsonDispatch(42, "Event.Accepted");
    result = instance.execute(script, "lua-event-capacity-first");
    RequireLuaSuccess(result);
    Eventually([&] { return target.BlockObserved.load(std::memory_order_acquire); });

    script = ProviderJsonDispatch(43, "Event.CapacityUnavailable");
    result = instance.execute(script, "lua-event-capacity-second");
    RequireLuaSuccess(result);

    target.Block.store(false, std::memory_order_release);
    target.Binding.NotifyCapacityChanged();
    Eventually([&] {
        return target.Seen.load(std::memory_order_acquire) == 42 &&
               target.Calls.load(std::memory_order_acquire) == 2;
    });

    assert(authorizer.Decisions >= 5);
    target.Binding.Shutdown();
    assert(runtime.Shutdown() == E::EventRuntimeStatus::Success);
    return 0;
}
