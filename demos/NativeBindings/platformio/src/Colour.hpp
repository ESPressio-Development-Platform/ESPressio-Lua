#pragma once
// This native type has no dependency on Lua or ESPressio.
struct Colour {
    int red, green, blue;
    Colour(int r, int g, int b) : red(r), green(g), blue(b) {}
    void clear() { red = green = blue = 0; }
    int brightness() const { return red; }
    void setBrightness(int value) { red = value; }
    int count() const { return 3; }
};
