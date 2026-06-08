#pragma once

#include <Arduino.h>

#include "rfid/card/rfid_card.h"

class NDEFManager {
public:
    static bool read(RFIDCard &card, String &outData, String &outError);

    static bool write(RFIDCard &card, const String &data, String &outError);

    static bool format(RFIDCard &card, String &outError);
};
