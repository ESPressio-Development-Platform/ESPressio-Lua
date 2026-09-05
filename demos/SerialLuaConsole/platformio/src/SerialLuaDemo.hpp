#pragma once
#include "LuaDeviceTypes.hpp"
#include "SerialConsole.hpp"

namespace SerialLua {
/// <summary>Owns hardware, reusable Lua definitions, VM and console in lifetime-safe order.</summary>
class SerialLuaDemo {
    // Application objects and captured writer are declared before the VM and outlive it.
    Led led_{Config::ledPin, Config::ledActiveLow};
    RgbLed rgb_{Config::redPin, Config::greenPin, Config::bluePin, Config::rgbActiveLow};
    Buzzer buzzer_{Config::buzzerPin};
    LuaSerialPrint print_;
    Lua::Type<Led> ledType_ = makeLedType();
    Lua::Type<RgbLed> rgbType_ = makeRgbLedType();
    Lua::Type<Buzzer> buzzerType_ = makeBuzzerType();
    Lua::Instance script_;
    LuaCommandHandler commands_{script_, print_};
    SerialConsole console_{commands_};
    bool ready_ = false;
    static bool check(const Lua::Result& result) {
        if (!result) { Serial.print("Initialization error: "); Serial.println(result.message); }
        return static_cast<bool>(result);
    }
public:
    /// <summary>Registers each encapsulated type in one line, then exposes the existing objects.</summary>
    void begin() {
        if (!check(script_.initializationResult())) return;
        if (!check(script_.registerType(ledType_))) return;
        if (!check(script_.registerType(rgbType_))) return;
        if (!check(script_.registerType(buzzerType_))) return;
        if (!check(script_.registerInstance("led", ledType_, led_, Lua::Borrowed))) return;
        if (!check(script_.registerInstance("rgb", rgbType_, rgb_, Lua::Borrowed))) return;
        if (!check(script_.registerInstance("buzzer", buzzerType_, buzzer_, Lua::Borrowed))) return;
        if (!check(print_.registerWith(script_))) return;
        ready_ = true;
        Serial.println("Serial Lua console ready. Outputs are OFF.");
        Serial.println("Enter: lua run print(led.isOn, rgb.isOn, buzzer.isOn)");
    }
    /// <summary>Services operator input only after registration has completed successfully.</summary>
    void poll() { if (ready_) console_.poll(); }
};
} // namespace SerialLua
