#pragma once
#include <Arduino.h>
#include <ESPressio_Lua.hpp>
#include "DemoConfig.hpp"

namespace SerialLua {
/// <summary>Installs variadic Lua print using tostring, tabs and a trailing newline.</summary>
/// <remarks>Must outlive the VM because the registered native writer captures this object.</remarks>
/**
 * ESPressio Memory Audit
 * Members:
 * - remaining_ (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class LuaSerialPrint {
    std::size_t remaining_ = Config::outputBudget;
public:
    /// <summary>Starts a fresh output allowance before executing an operator command.</summary>
    void resetBudget() { remaining_ = Config::outputBudget; }
    /// <summary>Registers the C++ Serial writer and a Lua print function for all Lua value types.</summary>
    ESPressio::Lua::Result registerWith(ESPressio::Lua::Instance& script) {
        auto result = script.registerFunction("demoSerialWrite", [this](std::string_view text) {
            if (text.size() > remaining_)
                throw ESPressio::Lua::BindingError("Serial output budget exceeded");
            remaining_ -= text.size();
            Serial.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
        });
        if (!result) return result;
        // Capture helpers so replacing a Lua global later does not change print's implementation.
        return script.execute(R"lua(
            local write, convert, selectArgument = demoSerialWrite, tostring, select
            function print(...)
                for i = 1, selectArgument('#', ...) do
                    if i > 1 then write('\t') end
                    write(convert(selectArgument(i, ...)))
                end
                write('\n')
            end
        )lua", "SerialPrint");
    }
};
} // namespace SerialLua
