#include <Arduino.h>
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR != 2
#error "This demo targets Arduino-ESP32 2.x LEDC APIs; install core 2.0.17."
#endif
#include <driver/gpio.h>
#include "SerialLuaDemo.hpp"

using SerialLua::Config;
static_assert(GPIO_IS_VALID_OUTPUT_GPIO(Config::ledPin) && GPIO_IS_VALID_OUTPUT_GPIO(Config::redPin) &&
              GPIO_IS_VALID_OUTPUT_GPIO(Config::greenPin) && GPIO_IS_VALID_OUTPUT_GPIO(Config::bluePin) &&
              GPIO_IS_VALID_OUTPUT_GPIO(Config::buzzerPin), "Configured GPIO must support output");

// Construct on first use in setup(), after Arduino has initialized hardware.
static SerialLua::SerialLuaDemo* demo = nullptr;

/// <summary>Initializes Serial and constructs the complete application with outputs OFF.</summary>
void setup() {
    Serial.begin(Config::baud);
    try {
        static SerialLua::SerialLuaDemo application;
        demo = &application;
        demo->begin();
    } catch (const std::exception& error) {
        Serial.print("Initialization error: "); Serial.println(error.what());
    } catch (...) { Serial.println("Initialization error: native exception"); }
}

/// <summary>Pumps complete operator commands and yields between polling iterations.</summary>
void loop() {
    if (demo) demo->poll();
    delay(1);
}
