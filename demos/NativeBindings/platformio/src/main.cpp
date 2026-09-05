#include <Arduino.h>
#include "BindingDemo.hpp"

void setup() {
    Serial.begin(115200);
    // Construct after any platform memory-provider installation your application requires.
    BindingDemo demo;
    const auto result = demo.run();
    if (!result) { Serial.println(result.message); return; }
    Serial.printf("Native colour: %d, %d, %d\n", demo.statusColour().red,
                  demo.statusColour().green, demo.statusColour().blue);
    // Expected: 42, 255, 128. Shutdown destroys Lua-owned objects but not borrowed ones.
}
void loop() { delay(1000); }
