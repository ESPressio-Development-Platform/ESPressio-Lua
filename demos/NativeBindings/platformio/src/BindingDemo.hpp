#pragma once
#include <ESPressio_Lua.hpp>
#include "Colour.hpp"

/// <summary>Owns the application object, its reusable Lua definition and an independent scripting VM.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - statusColour_ (Colour): 12 bytes [0 bytes dynamic allocation]
 * - colourType_ (ESPressio::Lua::Type<Colour>): 8 bytes [data_: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 48 bytes; data_: pointee: name: Capacity + 1 bytes backing buffer when allocated; data_: pointee: members: Capacity * (36 bytes) element storage; data_: pointee: members: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; data_: pointee: members: N live elements each: method: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; data_: pointee: members: N live elements each: getter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; data_: pointee: members: N live elements each: setter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; data_: pointee: constructors: Capacity * (12 bytes) element storage; data_: pointee: constructors: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * - script_ (ESPressio::Lua::Instance): 628 bytes [modules_: Capacity * (12 bytes) element storage; modules_: N live elements each: module: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 32 bytes; modules_: N live elements each: module: pointee: name: Capacity + 1 bytes backing buffer when allocated; modules_: N live elements each: module: pointee: symbols: Capacity * (24 bytes) element storage; modules_: N live elements each: module: pointee: symbols: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; modules_: N live elements each: module: pointee: symbols: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; types_: Capacity * (12 bytes) element storage; types_: N live elements each: type: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 48 bytes; types_: N live elements each: type: pointee: name: Capacity + 1 bytes backing buffer when allocated; types_: N live elements each: type: pointee: members: Capacity * (36 bytes) element storage; types_: N live elements each: type: pointee: members: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; types_: N live elements each: type: pointee: members: N live elements each: method: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; types_: N live elements each: type: pointee: members: N live elements each: getter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; types_: N live elements each: type: pointee: members: N live elements each: setter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; types_: N live elements each: type: pointee: constructors: Capacity * (12 bytes) element storage; types_: N live elements each: type: pointee: constructors: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; functions_: Capacity * (8 bytes) element storage; functions_: N live elements each: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Total Memory: 648 bytes [colourType_: data_: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 48 bytes; colourType_: data_: pointee: name: Capacity + 1 bytes backing buffer when allocated; colourType_: data_: pointee: members: Capacity * (36 bytes) element storage; colourType_: data_: pointee: members: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; colourType_: data_: pointee: members: N live elements each: method: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; colourType_: data_: pointee: members: N live elements each: getter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; colourType_: data_: pointee: members: N live elements each: setter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; colourType_: data_: pointee: constructors: Capacity * (12 bytes) element storage; colourType_: data_: pointee: constructors: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; script_: modules_: Capacity * (12 bytes) element storage; script_: modules_: N live elements each: module: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 32 bytes; script_: modules_: N live elements each: module: pointee: name: Capacity + 1 bytes backing buffer when allocated; script_: modules_: N live elements each: module: pointee: symbols: Capacity * (24 bytes) element storage; script_: modules_: N live elements each: module: pointee: symbols: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; script_: modules_: N live elements each: module: pointee: symbols: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; script_: types_: Capacity * (12 bytes) element storage; script_: types_: N live elements each: type: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 48 bytes; script_: types_: N live elements each: type: pointee: name: Capacity + 1 bytes backing buffer when allocated; script_: types_: N live elements each: type: pointee: members: Capacity * (36 bytes) element storage; script_: types_: N live elements each: type: pointee: members: N live elements each: name: Capacity + 1 bytes backing buffer when allocated; script_: types_: N live elements each: type: pointee: members: N live elements each: method: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; script_: types_: N live elements each: type: pointee: members: N live elements each: getter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; script_: types_: N live elements each: type: pointee: members: N live elements each: setter: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; script_: types_: N live elements each: type: pointee: constructors: Capacity * (12 bytes) element storage; script_: types_: N live elements each: type: pointee: constructors: N live elements each: call: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes; script_: functions_: Capacity * (8 bytes) element storage; script_: functions_: N live elements each: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 4 bytes]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
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
