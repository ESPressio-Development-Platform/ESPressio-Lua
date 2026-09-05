#pragma once
#include <ESPressio_Lua.hpp>
#include "OutputDevices.hpp"

namespace SerialLua {
namespace Lua = ESPressio::Lua;
/// <summary>Encapsulates the LED interface without exposing hardware construction to Lua.</summary>
inline Lua::Type<Led> makeLedType() {
    Lua::Type<Led> type{"Led"};
    type.property("isOn", &Led::isOn, &Led::setOn)
        .method("on", &Led::on).method("off", &Led::off).method("toggle", &Led::toggle);
    return type;
}
/// <summary>Encapsulates checked RGB channel properties and colour/off methods.</summary>
inline Lua::Type<RgbLed> makeRgbLedType() {
    Lua::Type<RgbLed> type{"RgbLed"};
    type.property("red", &RgbLed::red, &RgbLed::setRed)
        .property("green", &RgbLed::green, &RgbLed::setGreen)
        .property("blue", &RgbLed::blue, &RgbLed::setBlue)
        .readOnlyProperty("isOn", &RgbLed::isOn)
        .method("setColour", &RgbLed::setColour).method("off", &RgbLed::off);
    return type;
}
/// <summary>Encapsulates the buzzer's readable state and bounded, nonblocking tone control.</summary>
inline Lua::Type<Buzzer> makeBuzzerType() {
    Lua::Type<Buzzer> type{"Buzzer"};
    type.readOnlyProperty("isOn", &Buzzer::isOn)
        .readOnlyProperty("frequency", &Buzzer::frequency)
        .method("play", &Buzzer::play).method("off", &Buzzer::off);
    return type;
}
} // namespace SerialLua
