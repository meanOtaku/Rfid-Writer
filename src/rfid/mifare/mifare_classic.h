#pragma once

#include <Arduino.h>
#include "rfid/card/rfid_card.h"

class MifareClassic {
public:
    static bool readRawBlock(RFIDCard &card, uint8_t blockNum, String &outData, String &outError);
    static bool writeRawBlock(RFIDCard &card, uint8_t blockNum, const String &data, String &outError);
    static bool convertNdefToClassic(RFIDCard &card, String &outData, String &outError);
};
