#pragma once

#include <string_view>

#include <ESPressio_Lua.hpp>

#include "LuaSerialPrint.hpp"

namespace SerialLua {

/// <summary>Parses the demo's local `lua run` console grammar and executes Lua directly.</summary>
/// <remarks>
/// This is application-owned Serial UI syntax, not an ESPressio Command primitive. The
/// final Command architecture is asynchronous, typed intent and deliberately has no
/// mutable textual CommandRegistry. The demo therefore keeps its privileged operator
/// console local and synchronous instead of inventing a compatibility Command surface.
/// </remarks>
/**
 * ESPressio Memory Audit
 * Members:
 * - script_ (ESPressio::Lua::Instance&): 4 bytes [0 bytes dynamic allocation]
 * - print_ (LuaSerialPrint&): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 8 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t).
 * End ESPressio Memory Audit
 */
class LuaCommandHandler {
    ESPressio::Lua::Instance& script_;
    LuaSerialPrint& print_;

    static bool space(char c) noexcept { return c == ' ' || c == '\t'; }

    static std::string_view token(std::string_view& input) noexcept {
        while (!input.empty() && space(input.front())) input.remove_prefix(1);
        const auto end = input.find_first_of(" \t");
        const auto value = input.substr(0, end);
        input.remove_prefix(value.size());
        return value;
    }

    static ESPressio::Lua::Result usage(const char* message) noexcept {
        return ESPressio::Lua::Result::failure(ESPressio::Lua::Status::RuntimeError, message);
    }

public:
    /// <summary>Borrows the persistent Lua instance and bounded Serial print adapter.</summary>
    LuaCommandHandler(ESPressio::Lua::Instance& script, LuaSerialPrint& print) noexcept
        : script_(script), print_(print) {}

    /// <summary>Preserves Lua quoting/escaping by interpreting only the two console-prefix words.</summary>
    ESPressio::Lua::Result invoke(std::string_view line) {
        auto source = line;
        const auto group = token(source);
        const auto action = token(source);
        if (group != "lua" || action != "run")
            return usage("Usage: lua run <script>");

        // Consume only the required separator; preserve all remaining source bytes.
        if (source.empty()) return usage("Usage: lua run <script>");
        source.remove_prefix(1);
        if (source.find_first_not_of(" \t") == std::string_view::npos)
            return usage("Lua source is empty");

        print_.resetBudget();
        return script_.execute(source, "SerialConsole");
    }
};

} // namespace SerialLua
