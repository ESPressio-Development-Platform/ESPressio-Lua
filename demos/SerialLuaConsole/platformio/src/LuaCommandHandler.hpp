#pragma once
#include <ESPressio_Command.hpp>
#include <ESPressio_Lua.hpp>
#include "LuaSerialPrint.hpp"

namespace SerialLua {
/// <summary>Registers lua/run with ESPressio Command and adapts a raw Serial line to its invocation.</summary>
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
