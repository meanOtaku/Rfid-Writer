#pragma once

#include <Arduino.h>

enum class LEDStatus {
    Idle,
    Success,
    Error,
};

class StatusLED {
public:
    StatusLED(uint8_t yellowPin, uint8_t redPin, uint8_t greenPin);

    void begin();

    void set(LEDStatus status);

private:
    uint8_t yellowPin;
    uint8_t redPin;
    uint8_t greenPin;

    void write(bool yellowOn, bool redOn, bool greenOn);
};
