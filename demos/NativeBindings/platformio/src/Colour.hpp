#pragma once
// This native type has no dependency on Lua or ESPressio.
/**
 * ESPressio Memory Audit
 * Members:
 * - red (int): 4 bytes [0 bytes dynamic allocation]
 * - green (int): 4 bytes [0 bytes dynamic allocation]
 * - blue (int): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct Colour {
    int red, green, blue;
    Colour(int r, int g, int b) : red(r), green(g), blue(b) {}
    void clear() { red = green = blue = 0; }
    int brightness() const { return red; }
    void setBrightness(int value) { red = value; }
    int count() const { return 3; }
};
