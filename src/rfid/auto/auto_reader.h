#pragma once

#include <Arduino.h>

#include "rfid/card/rfid_card.h"

struct AutoReadResult {
    String format;
    String data;
};

class AutoReader {
public:
    static bool read(RFIDCard &card, AutoReadResult &result, String &outError);
};
