#pragma once
#include <cstddef>

// Override these in PlatformIO build_flags, or edit this header for Arduino IDE.
#ifndef DEMO_LED_PIN
#define DEMO_LED_PIN 23
#endif
#ifndef DEMO_RGB_RED_PIN
#define DEMO_RGB_RED_PIN 25
#endif
#ifndef DEMO_RGB_GREEN_PIN
#define DEMO_RGB_GREEN_PIN 26
#endif
#ifndef DEMO_RGB_BLUE_PIN
#define DEMO_RGB_BLUE_PIN 27
#endif
#ifndef DEMO_BUZZER_PIN
#define DEMO_BUZZER_PIN 32
#endif
#ifndef DEMO_LED_ACTIVE_LOW
#define DEMO_LED_ACTIVE_LOW 0
#endif
#ifndef DEMO_RGB_ACTIVE_LOW
#define DEMO_RGB_ACTIVE_LOW 0
#endif

namespace SerialLua {
/// <summary>Compile-time wiring and bounded console resources for a classic ESP32.</summary>
struct Config {
    static constexpr int ledPin = DEMO_LED_PIN;
    static constexpr int redPin = DEMO_RGB_RED_PIN;
    static constexpr int greenPin = DEMO_RGB_GREEN_PIN;
    static constexpr int bluePin = DEMO_RGB_BLUE_PIN;
    static constexpr int buzzerPin = DEMO_BUZZER_PIN;
    static constexpr bool ledActiveLow = DEMO_LED_ACTIVE_LOW != 0;
    static constexpr bool rgbActiveLow = DEMO_RGB_ACTIVE_LOW != 0;
    static constexpr unsigned long baud = 115200;
    static constexpr std::size_t lineCapacity = 1024; // Includes the trailing NUL.
    static constexpr std::size_t outputBudget = 4096; // Lua print bytes per command.
};
// Prevent two demo devices from driving the same pin.
static_assert(Config::ledPin != Config::redPin && Config::ledPin != Config::greenPin &&
              Config::ledPin != Config::bluePin && Config::ledPin != Config::buzzerPin &&
              Config::redPin != Config::greenPin && Config::redPin != Config::bluePin &&
              Config::redPin != Config::buzzerPin && Config::greenPin != Config::bluePin &&
              Config::greenPin != Config::buzzerPin && Config::bluePin != Config::buzzerPin,
              "Each output needs a distinct GPIO pin");
} // namespace SerialLua
