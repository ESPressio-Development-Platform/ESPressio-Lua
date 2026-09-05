#include "../demos/NativeBindings/arduino_ide/NativeBindings/BindingDemo.hpp"
#include <cstdio>
int main() {
    BindingDemo demo;
    const auto result = demo.run();
    if (!result) { std::fprintf(stderr, "%s\n", result.message); return 1; }
    const auto& colour = demo.statusColour();
    return colour.red == 42 && colour.green == 255 && colour.blue == 128 ? 0 : 2;
}
