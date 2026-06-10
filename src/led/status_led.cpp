#include "status_led.h"

StatusLED::StatusLED(uint8_t yellowPin, uint8_t redPin, uint8_t greenPin)
    : yellowPin(yellowPin), redPin(redPin), greenPin(greenPin) {}

void StatusLED::begin() {
    pinMode(yellowPin, OUTPUT);
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);

    set(LEDStatus::Idle);
}

void StatusLED::set(LEDStatus status) {
    switch (status) {
    case LEDStatus::Idle:
        write(true, false, false);
        break;

    case LEDStatus::Success:
        write(false, false, true);
        break;

    case LEDStatus::Error:
        write(false, true, false);
        break;
    }
}

void StatusLED::write(bool yellowOn, bool redOn, bool greenOn) {
    digitalWrite(yellowPin, yellowOn ? HIGH : LOW);
    digitalWrite(redPin, redOn ? HIGH : LOW);
    digitalWrite(greenPin, greenOn ? HIGH : LOW);
}
