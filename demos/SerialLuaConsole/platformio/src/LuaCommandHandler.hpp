#pragma once
#include <ESPressio_Command.hpp>
#include <ESPressio_Lua.hpp>
#include "LuaSerialPrint.hpp"

namespace SerialLua {
/// <summary>Registers lua/run with ESPressio Command and adapts a raw Serial line to its invocation.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - script_ (ESPressio::Lua::Instance&): 4 bytes [0 bytes dynamic allocation]
 * - print_ (LuaSerialPrint&): 4 bytes [0 bytes dynamic allocation]
 * - commands_ (ESPressio::Command::CommandRegistry): 160 bytes [root_: name_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: description_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: deprecationMessage_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: aliases_: Capacity * (24 bytes) element storage; root_: aliases_: N live elements each: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: children_: Capacity * (8 bytes) element storage; root_: children_: N live elements each: owned object: sizeof(CommandNode) (target/toolchain dependent); root_: parameters_: Capacity * (160 bytes) element storage; root_: parameters_: N live elements each: name_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: parameters_: N live elements each: description_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: parameters_: N live elements each: default_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: parameters_: N live elements each: validatorMessage_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: parameters_: N live elements each: aliases_: Capacity * (24 bytes) element storage; root_: parameters_: N live elements each: aliases_: N live elements each: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: parameters_: N live elements each: choices_: Capacity * (24 bytes) element storage; root_: parameters_: N live elements each: choices_: N live elements each: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; root_: before_: Capacity * (4 bytes) element storage; root_: after_: Capacity * (4 bytes) element storage; middleware_: Capacity * (4 bytes) element storage; observable_: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 52 bytes; observable_: pointee: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; observable_: pointee: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; observable_: pointee: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; observable_: pointee: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; observable_: pointee: Observable: _registrations: Capacity * (12 bytes) element storage; observable_: pointee: Observable: _bindings: Capacity * (12 bytes) element storage]
 * Total Memory: 168 bytes [commands_: root_: name_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: description_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: deprecationMessage_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: aliases_: Capacity * (24 bytes) element storage; commands_: root_: aliases_: N live elements each: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: children_: Capacity * (8 bytes) element storage; commands_: root_: children_: N live elements each: owned object: sizeof(CommandNode) (target/toolchain dependent); commands_: root_: parameters_: Capacity * (160 bytes) element storage; commands_: root_: parameters_: N live elements each: name_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: parameters_: N live elements each: description_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: parameters_: N live elements each: default_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: parameters_: N live elements each: validatorMessage_: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: parameters_: N live elements each: aliases_: Capacity * (24 bytes) element storage; commands_: root_: parameters_: N live elements each: aliases_: N live elements each: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: parameters_: N live elements each: choices_: Capacity * (24 bytes) element storage; commands_: root_: parameters_: N live elements each: choices_: N live elements each: CommandStringStorage: _value: Capacity + 1 bytes when capacity exceeds 15-byte SSO; commands_: root_: before_: Capacity * (4 bytes) element storage; commands_: root_: after_: Capacity * (4 bytes) element storage; commands_: middleware_: Capacity * (4 bytes) element storage; commands_: observable_: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 52 bytes; commands_: observable_: pointee: Observable: IUntypedObservable: IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; commands_: observable_: pointee: Observable: IUntypedObservable: IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; commands_: observable_: pointee: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; commands_: observable_: pointee: Observable: IUntypedObservable: IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; commands_: observable_: pointee: Observable: _registrations: Capacity * (12 bytes) element storage; commands_: observable_: pointee: Observable: _bindings: Capacity * (12 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class LuaCommandHandler {
    ESPressio::Lua::Instance& script_;
    LuaSerialPrint& print_;
    ESPressio::Command::CommandRegistry commands_;
    static bool space(char c) { return c == ' ' || c == '\t'; }
    static std::string_view token(std::string_view& input) {
        while (!input.empty() && space(input.front())) input.remove_prefix(1);
        const auto end = input.find_first_of(" \t");
        const auto value = input.substr(0, end);
        input.remove_prefix(value.size());
        return value;
    }
public:
    /// <summary>Creates a local registry and registers the script parameter and callback.</summary>
    LuaCommandHandler(ESPressio::Lua::Instance& script, LuaSerialPrint& print)
        : script_(script), print_(print) {
        using namespace ESPressio::Command;
        auto& run = commands_.Command("lua").Description("Lua scripting")
            .Command("run").Description("Execute the remaining line as Lua source");
        run.Parameter("script", ParameterKind::String).Description("Unmodified Lua source");
        run.OnExecute([this](const CommandContext& context) {
            print_.resetBudget();
            const auto& source = context.Raw("script");
            const auto result = script_.execute({source.data(), source.size()}, "SerialConsole");
            // The callback executes synchronously here; CommandResult is still a local disposition.
            return result ? CommandResult::Ok() : CommandResult::Error(result.message);
        });
    }
    /// <summary>Preserves Lua quoting/escaping by interpreting only the two command-path words.</summary>
    ESPressio::Command::CommandResult invoke(std::string_view line) {
        using namespace ESPressio::Command;
        auto source = line;
        const auto group = token(source);
        const auto action = token(source);
        if (group != "lua" || action != "run")
            return CommandResult::Error("Usage: lua run <script>");
        // Consume only the required separator; preserve remaining source whitespace verbatim.
        if (source.empty()) return CommandResult::Error("Usage: lua run <script>");
        source.remove_prefix(1);
        if (source.find_first_not_of(" \t") == std::string_view::npos)
            return CommandResult::Error("Lua source is empty");
        CommandInvocation invocation;
        invocation.path = {"lua", "run"};
        invocation.named[MakeCommandString("script")] = MakeCommandString(source);
        return commands_.Invoke(invocation); // Registry validates and dispatches the registered command.
    }
};
} // namespace SerialLua
