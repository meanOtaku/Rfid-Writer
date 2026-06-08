#pragma once

#include <Arduino.h>
#include "rfid/card/rfid_card.h"

class MifareClassic {
public:
    // Raw block operations
    static bool readRawBlock(RFIDCard &card, uint8_t blockNum, String &outData, String &outError);
    static bool writeRawBlock(RFIDCard &card, uint8_t blockNum, const String &data, String &outError);

    // NDEF operations (stubs/placeholder implementations)
    static bool readNDEF(RFIDCard &card, String &outData, String &outError);
    static bool writeNDEF(RFIDCard &card, const String &data, String &outError);
    static bool formatNDEF(RFIDCard &card, String &outError);
};
