#include <ESPressio_LuaCommand.hpp>

#include <ESPressio_Commands.hpp>
#include <ESPressio_RuntimeIdentity.hpp>
#include <ESPressio_SerializationMacros.hpp>
#include <HostRuntime.hpp>

#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>

using namespace ESPressio;
namespace C = ESPressio::Command;

namespace {

struct ScriptCommand final : C::SerializableCommand<ScriptCommand> {
    static constexpr C::CommandTypeId TypeId{0x8101};
    static constexpr const char* CanonicalName = "Test.Lua.ScriptCommand";
    static constexpr std::size_t MaximumLiveInstances = 2;
    static constexpr std::size_t MaximumPendingExecutions = 1;
    using ExecutionAdmissionPolicy = C::RequiredExecution;

    std::int32_t Value = 0;

    ESPRESSIO_SERIALIZABLE_TYPE(ScriptCommand)
    ESPRESSIO_SERIALIZABLE_SCHEMA_VERSION(1)
    ESPRESSIO_SERIALIZABLE_PROPERTIES(ESPRESSIO_PROPERTY("value", Value))
};

struct ScriptResponseCommand final : C::SerializableCommand<ScriptResponseCommand, std::uint32_t> {
    static constexpr C::CommandTypeId TypeId{0x8102};
    static constexpr const char* CanonicalName = "Test.Lua.ScriptResponseCommand";
    static constexpr std::size_t MaximumLiveInstances = 2;
    static constexpr std::size_t MaximumPendingExecutions = 1;
    static constexpr std::size_t MaximumPendingResponses = 2;
    using ExecutionAdmissionPolicy = C::RequiredExecution;

    std::int32_t Value = 0;

    ESPRESSIO_SERIALIZABLE_TYPE(ScriptResponseCommand)
    ESPRESSIO_SERIALIZABLE_SCHEMA_VERSION(1)
    ESPRESSIO_SERIALIZABLE_PROPERTIES(ESPRESSIO_PROPERTY("value", Value))
};

struct HandlerOwner final {
    std::atomic<std::int32_t> Seen{0};
    std::atomic<std::uint32_t> Calls{0};

    void Handle(const ScriptCommand& request, const C::CommandExecutionContext&) {
        Seen.store(request.Value, std::memory_order_release);
        Calls.fetch_add(1, std::memory_order_acq_rel);
    }
};

class Authorizer final : public Lua::ILuaCommandAuthorizer {
public:
    bool Allowed = true;
    mutable std::uint32_t Decisions = 0;

    Lua::LuaCommandAuthorizationDecision Authorize(
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept override {
        ++Decisions;
        assert(descriptor.Key.Family == C::CommandFamilyId);
        return Allowed
            ? Lua::LuaCommandAuthorizationDecision::Authorized
            : Lua::LuaCommandAuthorizationDecision::Denied;
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
    if (!result) std::fprintf(stderr, "Lua contract failure: %s\n", result.message);
    assert(result);
}

std::string ProviderJsonSubmission(std::int32_t value) {
    ScriptCommand request;
    request.Value = value;
    std::array<
        std::uint8_t,
        Serializable::MaximumSerializedSize<ScriptCommand, Serializable::JSON>> encoded{};
    const auto serialized = Serializable::SerializeBoundedJson(
        request, encoded.data(), encoded.size());
    assert(serialized);

    std::string script = "local status = Command.submitJson(1, [=[";
    script.append(reinterpret_cast<const char*>(encoded.data()), serialized.Bytes);
    script += "]=])\n";
    script += "assert(status == Command.Accepted, ";
    script += "\"status=\" .. tostring(status) .. \" accepted=\" .. tostring(Command.Accepted))";
    return script;
}

System::DeviceRuntimeIdentity Identity() {
    System::DeviceIdentifier::Storage bytes{};
    bytes[0] = 0x51;
    return {System::DeviceIdentifier{bytes}, System::RuntimeIncarnationId{0x91}};
}

} // namespace

int main() {
    HostRuntime platform;
    assert(System::RuntimeIdentity::Install(Identity()) ==
           System::RuntimeIdentity::InstallationStatus::Success);

    Primitive::TypeDirectory<1> runtimeDirectory;
    assert(runtimeDirectory.Register<ScriptCommand>() ==
           Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(runtimeDirectory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);

    C::RuntimeConfiguration runtimeConfiguration{};
    runtimeConfiguration.ExecutionLane.Name = "luaCommandLane";
    runtimeConfiguration.ExecutionLane.StackSize = 4096;
    C::Runtime runtime(runtimeConfiguration);
    HandlerOwner owner;
    assert(runtime.BindHandler<ScriptCommand>(owner, &HandlerOwner::Handle) ==
           C::CommandRuntimeStatus::Success);
    assert(runtime.Initialize(runtimeDirectory.View()) == C::CommandRuntimeStatus::Success);
    assert(runtime.Start() == C::CommandRuntimeStatus::Success);

    Primitive::TypeDirectory<2> luaDirectory;
    assert(luaDirectory.Register<ScriptResponseCommand>() ==
           Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(luaDirectory.Register<ScriptCommand>() ==
           Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(luaDirectory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);
    assert(luaDirectory.View().begin()[0].Key.TypeValue == ScriptCommand::TypeId.Value());
    assert(luaDirectory.View().begin()[1].Key.TypeValue == ScriptResponseCommand::TypeId.Value());

    Authorizer authorizer;

    Lua::Instance unavailableInstance;
    assert(unavailableInstance.initializationResult());
    assert(Lua::RegisterCommandAdmission(
               unavailableInstance, Primitive::TypeDirectoryView{}, authorizer).status ==
           Lua::Status::InvalidState);

    Lua::Instance instance;
    assert(instance.initializationResult());
    assert(Lua::RegisterCommandAdmission(instance, luaDirectory.View(), authorizer));

    auto providerJson = ProviderJsonSubmission(41);
    auto result = instance.execute(providerJson, "lua-command-json");
    RequireLuaSuccess(result);
    Eventually([&] {
        return owner.Seen.load(std::memory_order_acquire) == 41 &&
               owner.Calls.load(std::memory_order_acquire) == 1;
    });

    result = instance.execute(R"LUA(
        assert(Command.submitJson(1, [[not-json]]) == Command.SchemaOrDecodeFailure)
    )LUA", "lua-command-invalid-schema");
    RequireLuaSuccess(result);
    assert(owner.Calls.load(std::memory_order_acquire) == 1);

    authorizer.Allowed = false;
    result = instance.execute(R"LUA(
        local ok = pcall(function()
            Command.submitJson(1, [[]])
        end)
        assert(ok == false)
    )LUA", "lua-command-authorization");
    RequireLuaSuccess(result);
    assert(owner.Calls.load(std::memory_order_acquire) == 1);

    authorizer.Allowed = true;
    result = instance.execute(R"LUA(
        local responseOk = pcall(function()
            Command.submitJson(2, [[]])
        end)
        assert(responseOk == false)

        local indexOk = pcall(function()
            Command.submitJson(3, [[]])
        end)
        assert(indexOk == false)

        local oversizedOk = pcall(function()
            Command.submitJson(1, string.rep("x", 4096))
        end)
        assert(oversizedOk == false)
    )LUA", "lua-command-boundaries");
    RequireLuaSuccess(result);
    assert(owner.Calls.load(std::memory_order_acquire) == 1);
    assert(authorizer.Decisions >= 4);

    assert(runtime.Shutdown() == C::CommandRuntimeStatus::Success);
    return 0;
}
