#include <ESPressio_LuaState.hpp>

#include <ESPressio_RuntimeIdentity.hpp>
#include <ESPressio_SerializationMacros.hpp>
#include <ESPressio_States.hpp>
#include <HostRuntime.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

using namespace ESPressio;
namespace S = ESPressio::State;

namespace {

struct ScriptValue final {
    std::uint32_t Value = 0;
    constexpr bool operator==(const ScriptValue& other) const noexcept { return Value == other.Value; }
    ESPRESSIO_SERIALIZABLE_TYPE(ScriptValue)
    ESPRESSIO_SERIALIZABLE_SCHEMA_VERSION(1)
    ESPRESSIO_SERIALIZABLE_PROPERTIES(ESPRESSIO_PROPERTY("value", Value))
};

struct ScriptState final : S::SerializableState<ScriptState, ScriptValue> {
    static constexpr S::StateTypeId TypeId{0x8301};
    static constexpr const char* CanonicalName = "Test.Lua.ScriptState";
};

struct LocalState final : S::State<LocalState, std::uint32_t> {
    static constexpr S::StateTypeId TypeId{0x8302};
    static constexpr const char* CanonicalName = "Test.Lua.LocalState";
};

class Authorizer final : public Lua::ILuaStateAuthorizer {
public:
    bool Allowed = true;
    mutable std::uint32_t Decisions = 0;

    Lua::LuaStateAuthorizationDecision Authorize(
        const Primitive::PrimitiveTypeDescriptor& descriptor
    ) const noexcept override {
        ++Decisions;
        assert(descriptor.Key.Family == S::StateFamilyId);
        return Allowed ? Lua::LuaStateAuthorizationDecision::Authorized
                       : Lua::LuaStateAuthorizationDecision::Denied;
    }
};

Timing::QualifiedTime CaptureTruthTime() {
    return {0x0000000200000003ULL, Timing::TimeReliability::Holdover};
}

System::DeviceRuntimeIdentity Identity() {
    System::DeviceIdentifier::Storage bytes{};
    bytes[0] = 0x71;
    return {System::DeviceIdentifier{bytes}, System::RuntimeIncarnationId{0x93}};
}

void RequireLuaSuccess(const Lua::Result& result) {
    if (!result) std::fprintf(stderr, "Lua State contract failure: %s\n", result.message);
    assert(result);
}

template<class Format>
std::string EncodeExpected(const ScriptValue& value) {
    std::array<std::uint8_t, Serializable::MaximumSerializedSize<ScriptValue, Format>> bytes{};
    const auto encoded = [&] {
        if constexpr (std::is_same_v<Format, Serializable::DirectBinary>) {
            return Serializable::SerializeDirectBinary(value, bytes.data(), bytes.size());
        } else if constexpr (std::is_same_v<Format, Serializable::CBOR>) {
            return Serializable::SerializeBoundedCbor(value, bytes.data(), bytes.size());
        } else {
            static_assert(std::is_same_v<Format, Serializable::JSON>);
            return Serializable::SerializeBoundedJson(value, bytes.data(), bytes.size());
        }
    }();
    assert(encoded);
    return {reinterpret_cast<const char*>(bytes.data()), encoded.Bytes};
}

} // namespace

int main() {
    HostRuntime platform;
    assert(System::RuntimeIdentity::Install(Identity()) ==
           System::RuntimeIdentity::InstallationStatus::Success);

    Primitive::TypeDirectory<2> directory;
    assert(directory.Register<ScriptState>() == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Register<LocalState>() == Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Initialize() == Primitive::TypeDirectoryInitializationStatus::Success);
    assert(directory.View().begin()[0].Key.TypeValue == ScriptState::TypeId.Value());
    assert(directory.View().begin()[1].Key.TypeValue == LocalState::TypeId.Value());

    S::Runtime<S::TypeConfiguration<ScriptState>, S::TypeConfiguration<LocalState>> runtime;
    auto scriptOwner = runtime.BindOwner<ScriptState>();
    auto localOwner = runtime.BindOwner<LocalState>();
    assert(scriptOwner && localOwner);
    assert(runtime.Initialize(directory.View(), &CaptureTruthTime) == S::StateRuntimeStatus::Success);
    assert(runtime.Start() == S::StateRuntimeStatus::Success);

    Authorizer authorizer;
    Lua::Instance unavailable;
    assert(unavailable.initializationResult());
    assert(Lua::RegisterStateInspection<512>(
               unavailable, Primitive::TypeDirectoryView{}, authorizer).status == Lua::Status::InvalidState);

    Lua::Instance instance;
    assert(instance.initializationResult());
    assert(Lua::RegisterStateInspection<512>(instance, directory.View(), authorizer));
    assert(Lua::RegisterStateInspection<1>(instance, directory.View(), authorizer, "TinyState"));

    auto result = instance.execute(R"LUA(
        local r = State.readJson(1)
        assert(r.status == State.NoValue)
        assert(r.payload == nil)
        assert(State.set == nil and State.write == nil and State.owner == nil)
    )LUA", "lua-state-no-value");
    RequireLuaSuccess(result);

    assert(scriptOwner.Set({42}) == S::StateSetStatus::Changed);
    const ScriptValue expectedValue{42};
    const auto expectedDirect = EncodeExpected<Serializable::DirectBinary>(expectedValue);
    const auto expectedCbor = EncodeExpected<Serializable::CBOR>(expectedValue);
    const auto expectedJson = EncodeExpected<Serializable::JSON>(expectedValue);

    Lua::Module verify("Verify");
    verify.function("direct", [&expectedDirect](std::string_view bytes) { return bytes == expectedDirect; });
    verify.function("cbor", [&expectedCbor](std::string_view bytes) { return bytes == expectedCbor; });
    verify.function("json", [&expectedJson](std::string_view bytes) { return bytes == expectedJson; });
    verify.constant("Holdover", Timing::TimeReliability::Holdover);
    assert(instance.registerModule(verify));

    result = instance.execute(R"LUA(
        local d = State.readDirectBinary(1)
        local c = State.readCbor(1)
        local j = State.readJson(1)
        for _, r in ipairs({d, c, j}) do
            assert(r.status == State.Success)
            assert(r.truthNanosecondsHigh32 == 2)
            assert(r.truthNanosecondsLow32 == 3)
            assert(r.truthReliability == Verify.Holdover)
        end
        assert(Verify.direct(d.payload))
        assert(Verify.cbor(c.payload))
        assert(Verify.json(j.payload))
    )LUA", "lua-state-read-formats");
    RequireLuaSuccess(result);

    authorizer.Allowed = false;
    result = instance.execute(R"LUA(
        local ok = pcall(function() State.readJson(1) end)
        assert(ok == false)
    )LUA", "lua-state-authorization");
    RequireLuaSuccess(result);

    authorizer.Allowed = true;
    result = instance.execute(R"LUA(
        local localOk = pcall(function() State.readJson(2) end)
        local indexOk = pcall(function() State.readJson(3) end)
        assert(localOk == false)
        assert(indexOk == false)
        local tiny = TinyState.readJson(1)
        assert(tiny.status == TinyState.InsufficientOutput)
        assert(tiny.payload == nil)
    )LUA", "lua-state-boundaries");
    RequireLuaSuccess(result);

    S::StateSnapshot<ScriptState> snapshot{};
    assert(S::StateTypeRuntime<ScriptState>::Get().TryRead(snapshot));
    assert(snapshot.Value.Value == 42);
    assert(authorizer.Decisions >= 6);
    assert(runtime.Shutdown() == S::StateRuntimeStatus::Success);
    return 0;
}
