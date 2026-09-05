#pragma once
#include <ESPressio_Lua.hpp>
#include "Colour.hpp"

/// <summary>Owns the application object, its reusable Lua definition and an independent scripting VM.</summary>
class BindingDemo {
    // Declaration order makes the borrowed object outlive the VM, including shutdown finalizers.
    Colour statusColour_{0, 255, 0};
    ESPressio::Lua::Type<Colour> colourType_{"Colour"};
    ESPressio::Lua::Instance script_;
public:
    /// <summary>Defines the interface once, installs it, and runs the embedded text script.</summary>
    ESPressio::Lua::Result run() {
        namespace Lua = ESPressio::Lua;
        if (!script_.initializationResult()) return script_.initializationResult();
        colourType_.constructor<int, int, int>()
            .field("red", &Colour::red).field("green", &Colour::green).field("blue", &Colour::blue)
            .method("clear", &Colour::clear)
            .property("brightness", &Colour::brightness, &Colour::setBrightness)
            .readOnlyProperty("count", &Colour::count);
        auto result = script_.registerType(colourType_); // First registration freezes the shared definition.
        if (!result) return result;
        result = script_.registerInstance("statusColour", colourType_, statusColour_, Lua::Borrowed);
        if (!result) return result;
        Lua::Module application{"app"};
        application.constant("MAX_BRIGHTNESS", 255).function("double", [](int value) { return value * 2; });
        result = script_.registerModule(application); // The VM retains this definition after run returns.
        if (!result) return result;
        result = script_.execute(R"lua(
            local colour = Colour(0, 128, 255) -- Lua owns this new native object.
            colour.brightness = app.double(32)
            assert(colour.red == 64 and colour.count == 3)
            colour:clear()
            assert(colour.red == 0)
            statusColour.blue = 128 -- Mutates the borrowed application object.
            assert(app.MAX_BRIGHTNESS == 255)
            function setRed(value) statusColour.red = value end
        )lua", "NativeBindings");
        if (!result) return result;
        return script_.call("setRed", 42); // Host-to-Lua invocation uses the same protected execution boundary.
    }
    /// <summary>Inspects the native application object after the script changes it.</summary>
    const Colour& statusColour() const noexcept { return statusColour_; }
};
