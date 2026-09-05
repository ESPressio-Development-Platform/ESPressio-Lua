#pragma once
#include <Arduino.h>
#include <stdexcept>
#include "DemoConfig.hpp"

// This demo application owns the Arduino GPIO implementation; the Lua library stays hardware-neutral.
namespace SerialLua {
/// <summary>On/off LED with optional active-low wiring and readable logical state.</summary>
class Led {
    int pin_;
    bool activeLow_;
    bool on_ = false;
public:
    /// <summary>Configures the output OFF; construct after Arduino hardware initialization.</summary>
    Led(int pin, bool activeLow) : pin_(pin), activeLow_(activeLow) {
        digitalWrite(pin_, activeLow_ ? HIGH : LOW); // Preload the inactive level.
        pinMode(pin_, OUTPUT);
        off();
    }
    /// <summary>Returns the last commanded logical state.</summary>
    bool isOn() const { return on_; }
    /// <summary>Sets the logical state, accounting for the configured polarity.</summary>
    void setOn(bool value) {
        digitalWrite(pin_, (value != activeLow_) ? HIGH : LOW);
        on_ = value;
    }
    /// <summary>Turns the LED on.</summary>
    void on() { setOn(true); }
    /// <summary>Turns the LED off.</summary>
    void off() { setOn(false); }
    /// <summary>Inverts the logical state.</summary>
    void toggle() { setOn(!on_); }
};

/// <summary>Three-channel PWM RGB LED; channel values are integers from 0 to 255.</summary>
class RgbLed {
    bool activeLow_;
    int red_ = 0, green_ = 0, blue_ = 0;
    // RGB uses channels 0/1 (timer 0) and 2 (timer 1). Buzzer uses channel 4 (timer 2).
    void write(unsigned channel, int value) {
        ledcWrite(channel, activeLow_ ? 255 - value : value);
    }
    static void validate(int value) {
        if (value < 0 || value > 255) throw std::out_of_range("RGB channel must be 0..255");
    }
    void attach(int pin, unsigned channel) {
        digitalWrite(pin, activeLow_ ? HIGH : LOW);
        pinMode(pin, OUTPUT);
        if (ledcSetup(channel, 5000, 8) == 0) throw std::runtime_error("RGB PWM setup failed");
        write(channel, 0); // Set OFF duty before connecting the PWM output.
        ledcAttachPin(pin, channel);
    }
public:
    /// <summary>Attaches three GPIO outputs, initially with all channels OFF.</summary>
    RgbLed(int redPin, int greenPin, int bluePin, bool activeLow) : activeLow_(activeLow) {
        attach(redPin, 0); attach(greenPin, 1); attach(bluePin, 2);
    }
    /// <summary>Returns the commanded red intensity.</summary>
    int red() const { return red_; }
    /// <summary>Returns the commanded green intensity.</summary>
    int green() const { return green_; }
    /// <summary>Returns the commanded blue intensity.</summary>
    int blue() const { return blue_; }
    /// <summary>Reports whether any channel has nonzero intensity.</summary>
    bool isOn() const { return red_ != 0 || green_ != 0 || blue_ != 0; }
    /// <summary>Updates all channels after validating every argument.</summary>
    void setColour(int red, int green, int blue) {
        validate(red); validate(green); validate(blue); // Invalid input changes no channel.
        write(0, red); write(1, green); write(2, blue);
        red_ = red; green_ = green; blue_ = blue;
    }
    /// <summary>Sets red intensity while preserving the other channels.</summary>
    void setRed(int value) { setColour(value, green_, blue_); }
    /// <summary>Sets green intensity while preserving the other channels.</summary>
    void setGreen(int value) { setColour(red_, value, blue_); }
    /// <summary>Sets blue intensity while preserving the other channels.</summary>
    void setBlue(int value) { setColour(red_, green_, value); }
    /// <summary>Turns all three channels off.</summary>
    void off() { setColour(0, 0, 0); }
};

/// <summary>Passive buzzer driven by an independent PWM timer; initially silent.</summary>
class Buzzer {
    int frequency_ = 0;
public:
    /// <summary>Configures the buzzer GPIO and PWM channel in the OFF state.</summary>
    explicit Buzzer(int pin) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
        if (ledcSetup(4, 2000, 10) == 0) throw std::runtime_error("Buzzer PWM setup failed");
        ledcWrite(4, 0);
        ledcAttachPin(pin, 4);
    }
    /// <summary>Returns true when a continuous tone has been requested.</summary>
    bool isOn() const { return frequency_ != 0; }
    /// <summary>Returns the requested tone frequency, or zero when OFF.</summary>
    int frequency() const { return frequency_; }
    /// <summary>Starts a continuous 20..20000 Hz tone; returns immediately.</summary>
    void play(int frequency) {
        if (frequency < 20 || frequency > 20000)
            throw std::out_of_range("Buzzer frequency must be 20..20000 Hz");
        if (ledcWriteTone(4, frequency) == 0) throw std::runtime_error("Buzzer tone setup failed");
        frequency_ = frequency;
    }
    /// <summary>Stops the tone and drives the output low.</summary>
    void off() { ledcWrite(4, 0); frequency_ = 0; }
};
} // namespace SerialLua
