#pragma once
#include <Arduino.h>
#include <exception>
#include "LuaCommandHandler.hpp"

namespace SerialLua {
/// <summary>Bounded, nonblocking Serial line reader with CR/LF and backspace handling.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - handler_ (LuaCommandHandler&): 4 bytes [0 bytes dynamic allocation]
 * - line_ (char[Config::lineCapacity]): 1024 bytes [0 bytes dynamic allocation]
 * - used_ (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - overflow_ (bool): 1 bytes [0 bytes dynamic allocation]
 * - previousCR_ (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 1036 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class SerialConsole {
    LuaCommandHandler& handler_;
    char line_[Config::lineCapacity]{};
    std::size_t used_ = 0;
    bool overflow_ = false;
    bool previousCR_ = false;
    void finish() {
        if (overflow_) Serial.println("ERROR: line too long; discarded without execution");
        else if (used_ != 0) {
            try {
                const auto result = handler_.invoke({line_, used_});
                if (result.success) Serial.println("OK");
                else { Serial.print("ERROR: "); Serial.println(result.message.c_str()); }
            } catch (const std::exception& error) {
                Serial.print("ERROR: "); Serial.println(error.what());
            } catch (...) { Serial.println("ERROR: native exception"); }
        }
        used_ = 0;
        overflow_ = false;
    }
public:
    /// <summary>Borrows the application's registered command handler.</summary>
    explicit SerialConsole(LuaCommandHandler& handler) : handler_(handler) {}
    /// <summary>Consumes at most 128 bytes per loop iteration; never executes a truncated line.</summary>
    void poll() {
        for (unsigned count = 0; count < 128 && Serial.available() > 0; ++count) {
            const int incoming = Serial.read();
            if (incoming < 0) break;
            const char c = static_cast<char>(incoming);
            if (c == '\n' && previousCR_) { previousCR_ = false; continue; }
            previousCR_ = c == '\r';
            if (c == '\r' || c == '\n') { finish(); continue; }
            if (overflow_) continue; // Drain the complete bad line, including its terminator.
            if (c == '\b' || c == 127) { if (used_ != 0) --used_; continue; }
            if (c == '\0') { overflow_ = true; continue; } // Reject binary input.
            if (used_ == sizeof(line_) - 1) { overflow_ = true; continue; }
            line_[used_++] = c;
        }
    }
};
} // namespace SerialLua
